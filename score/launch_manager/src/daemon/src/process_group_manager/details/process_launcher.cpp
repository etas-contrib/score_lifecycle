/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#include "score/mw/launch_manager/common/log.hpp"
#include "score/mw/launch_manager/common/signal_safe_log.hpp"
#include "score/mw/launch_manager/osal/ipc_comms.hpp"
#include "score/mw/launch_manager/osal/start_process.hpp"
#include "score/mw/launch_manager/process_group_manager/details/process_launcher.hpp"
#include "score/mw/launch_manager/process_group_manager/iprocess.hpp"
#include <charconv>
#include <array>
#include <cerrno>
#include <cstdio>
#include <iterator>
#include <optional>
#include <string_view>
#include <utility>

constexpr int kPidZero = 0;  // This value is used to check if the process ID (uses pid_t) is valid or not.
constexpr int kPosixSuccess = 0;

namespace score::mw::lifecycle::internal::osal
{

ProcessLauncher::ProcessLauncher(std::unique_ptr<IProcessStarter> process_starter)
    : process_starter_(std::move(process_starter))
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(process_starter_ != nullptr, "The process starter must not be null");
}

OsalReturnType
ProcessLauncher::startProcess(ProcessID& pid, IpcCommsP& block, const configuration::ComponentConfig& config)
{
    OsalReturnType result = OsalReturnType::kFail;

    if (!config.component_properties.binary_name.empty())
    {
        if (access(config.deployment_config.process_command.executablePath().c_str(), X_OK) != 0)
        {
            static_cast<void>(signal_safe_log(
                "File does not exist or is not executable: ",
                config.deployment_config.process_command.executablePath()));
            return result;
        }

        int fd = -1;
        pid = -1;
        block = nullptr;
        bool comms_result = true;

        const auto& app_type = config.component_properties.application_profile.application_type;
        if (app_type != configuration::ApplicationType::Native)
        {
            comms_result = setupComms(block, fd, config);
        }

        if (comms_result)
        {
            if (fflush(stdout) != 0)
            {
                LM_LOG_ERROR() << "Failed to flush stdout before starting the process.";
            }

            result = process_starter_->startProcess(pid, config, fd, block);

            if (result != OsalReturnType::kSuccess)
            {
                LM_LOG_ERROR() << "Process creation failed: Unable to create a new process.";
            }
        }
        else
        {
            LM_LOG_ERROR()
                << "Shared memory creation failed: Unable to create shared memory for kRunning communication.";
        }

        if (fd >= 0)
        {
            close(fd);
        }
    }
    else
    {
        LM_LOG_ERROR() << "Invalid input parameters: Ensure config and binary_name are correctly provided.";

        return result;
    }

    return result;
}

bool ProcessLauncher::setupComms(IpcCommsP& block, int& fd, const configuration::ComponentConfig& config)
{
    size_t length = sizeof(IpcCommsSync);

    constexpr std::string_view kShmNamePrefix{"/ipc_shared_mem"};
    std::array<char, static_cast<std::size_t>(ProcessLimits::maxLocalBuffSize)> shm_name{};
    static_cast<void>(kShmNamePrefix.copy(shm_name.data(), kShmNamePrefix.size()));

    char* const counter_first = std::next(shm_name.data(), static_cast<std::ptrdiff_t>(kShmNamePrefix.size()));
    // The last byte is excluded from the conversion range so that it keeps the zero from the value
    // initialization above, which guarantees a null-terminated name for shm_open().
    char* const counter_last = std::next(shm_name.data(), static_cast<std::ptrdiff_t>(shm_name.size() - 1U));

    const auto conversion = std::to_chars(counter_first, counter_last, shm_name_counter++);
    if (conversion.ec != std::errc{})
    {
        LM_LOG_ERROR() << "Shared memory name truncated: the local buffer is too small for the shared memory "
                          "object name.";
        return false;
    }
    *conversion.ptr = '\0';

    fd = shm_open(shm_name.data(), O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0U);
    if (fd < 0)
    {
        LM_LOG_ERROR() << "shm_open failed:" << config.deployment_config.process_command.executablePath()
                       << "Unable to open shared memory object. Error:" << errno_message(errno);
        return false;
    }

    shm_unlink(shm_name.data());

    if (-1 == ftruncate(fd, static_cast<int>(length)))
    {
        LM_LOG_ERROR() << "ftruncate failed:" << config.deployment_config.process_command.executablePath()
                       << "Unable to set size of shared memory file descriptor. Error:" << errno_message(errno);
        return false;
    }

    block = IpcCommsSync::getCommsObject(fd);
    if (!block)
    {
        return false;
    }

    // Map application type to CommsType for backward compatibility
    SCORE_LANGUAGE_FUTURECPP_ASSERT_DBG_MESSAGE(
        config.component_properties.application_profile.application_type != configuration::ApplicationType::Native,
        "We should not set up the comms object if the application type is native. This used to be the kNoComms case");

    block->comms_type_ = CommsType::kReporting;

    if (!IpcCommsSync::initializeSemaphores(block))
    {
        LM_LOG_ERROR() << "Semaphore init failed:" << config.name
                       << "Unable to initialize send_sync or reply_sync semaphore.";
        return false;
    }

    return true;
}

OsalReturnType ProcessLauncher::requestTermination(ProcessID pid)
{
    LM_LOG_DEBUG() << "Request termination received for" << pid;

    OsalReturnType result = OsalReturnType::kFail;

    if (pid > kPidZero)
    {
        if (kill(pid, SIGTERM) == kPosixSuccess)
        {
            result = OsalReturnType::kSuccess;
        }
        else
        {
            LM_LOG_ERROR() << "SIGTERM failed: Unable to send SIGTERM to process ID" << pid
                           << ". Error:" << errno_message(errno);
        }
    }
    else
    {
        LM_LOG_ERROR() << "Invalid process ID: The process ID" << pid << "is invalid.";
    }

    return result;
}

OsalReturnType ProcessLauncher::forceTermination(ProcessID pid)
{
    LM_LOG_DEBUG() << "Forced termination received for pid" << pid;

    OsalReturnType result = OsalReturnType::kFail;

    if (pid > kPidZero)
    {
        if (kill(pid, SIGKILL) == kPosixSuccess)
        {
            result = OsalReturnType::kSuccess;
        }
        else if (errno == ESRCH)
        {
            LM_LOG_WARN() << "SIGKILL failed: Process is already gone (ESRCH) for process ID" << pid;
        }
        else
        {
            LM_LOG_FATAL() << "SIGKILL failed: Unable to send SIGKILL to process ID" << pid;
        }
    }
    else
    {
        LM_LOG_ERROR() << "Invalid process ID: The process ID" << pid << "is invalid.";
    }

    return result;
}

OsalReturnType ProcessLauncher::waitForTermination(osal::ProcessID& pid, int32_t& status)
{
    int32_t wait_status;
    osal::OsalReturnType result = osal::OsalReturnType::kFail;

    pid_t terminated_pid = wait(&wait_status);

    if (terminated_pid > 0)
    {
        result = osal::OsalReturnType::kSuccess;
        pid = terminated_pid;
        status = wait_status;
    }
    else
    {
        /// exiting with pid == 0 is perfectly normal behaviour when all process groups are in the Off state.
        LM_LOG_DEBUG() << "wait failed: Unable to wait for any child process to terminate. Error:"
                       << errno_message(errno);
    }

    return result;
}

OsalReturnType ProcessLauncher::waitForkRunning(IpcCommsP sync, std::optional<std::chrono::milliseconds> timeout)
{
    OsalReturnType result = OsalReturnType::kSuccess;

    if (!sync)
    {
        LM_LOG_ERROR() << "Invalid shared memory pointer: The shared memory pointer is null.";
        return OsalReturnType::kFail;
    }

    if (timeout.has_value())
    {
        result = sync->send_sync_.timedWait(timeout.value());
    }

    const auto post_res = sync->reply_sync_.post();

    if (post_res == OsalReturnType::kFail)
    {
        LM_LOG_ERROR() << "Semaphore post failed";
        result = OsalReturnType::kFail;
    }

    if (result == OsalReturnType::kFail)
    {
        LM_LOG_ERROR() << "Semaphore timedWait or post failed: Unable to wait or post on semaphores within the "
                          "specified timeout.";
    }
    else if (timeout.has_value())
    {
        result = sync->send_sync_.timedWait(std::chrono::milliseconds(100));
    }

    IpcCommsSync::deinit(sync);

    return result;
}

}  // namespace score::mw::lifecycle::internal::osal
