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

#include <string_view>

#include <fcntl.h>
#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>

#include "score/mw/launch_manager/common/log.hpp"
#include "score/mw/launch_manager/common/signal_safe_log.hpp"
#include "score/mw/launch_manager/osal/set_affinity.hpp"
#include "score/mw/launch_manager/osal/set_groups.hpp"
#include "score/mw/launch_manager/osal/start_process.hpp"
#include "score/mw/launch_manager/osal/sys_exit.hpp"
#include <cassert>
#include <cerrno>
#include <cstdlib>

namespace score::mw::lifecycle::internal::osal
{

namespace
{

/// @brief Struct to hold configuration parameters for the child process.
struct ChildProcessConfig
{
    const configuration::ComponentConfig& config;  ///< child process startup configurations
    int fd;                                        ///< fd File descriptor of the shared memory segment.
    IpcCommsP shared_block;                        ///< sync Pointer to the shared memory block.
};

/// @brief Applies the given limit.
/// @details The implementation should be async signal safe.
/// @warning This will sysexit if the set is not succesful.
void applyLimitOrDie(const int resource, const rlimit& limit, const std::string_view rlimit_name) noexcept(false)
{
    if (::setrlimit(resource, &limit) == -1)
    {
        static_cast<void>(signal_safe_log_errno(errno, "Failed to apply rlimit ", rlimit_name));
        sysexit(EXIT_FAILURE);
    }
}

/// @brief Returns the RLIMIT_* macro name for the given platform resource constant, for use in
/// log messages only.
std::string_view rlimitResourceName(const int resource) noexcept
{
    switch (resource)
    {
        case RLIMIT_AS:
            return "RLIMIT_AS";
        case RLIMIT_CORE:
            return "RLIMIT_CORE";
        case RLIMIT_CPU:
            return "RLIMIT_CPU";
        case RLIMIT_DATA:
            return "RLIMIT_DATA";
        case RLIMIT_FSIZE:
            return "RLIMIT_FSIZE";
        case RLIMIT_NOFILE:
            return "RLIMIT_NOFILE";
        case RLIMIT_STACK:
            return "RLIMIT_STACK";
        default:
            return "RLIMIT_<unknown>";
    }
}

/// @details The implementation should be async signal safe.
void handleComms(ChildProcessConfig& param)
{
    if (!param.shared_block)
    {
        // kNoComms, fds are CLOEXEC
        return;
    }

    param.fd = dup2(param.fd, param.shared_block->sync_fd);  // always make sure we are using fd=3
    param.shared_block->pid_ = getpid();                     // Store pid for check at client end

    // It must be ensured that sync_fd (f3) remains open depending on
    // the communication type. Flag FD_CLOEXEC is cleared conditionally to ensure that the
    // respective file descriptor remains open after the execve call.
    assert(
        param.shared_block->comms_type_ != CommsType::kNoComms &&
        "Invalid comms type, kNoComms is expected to be handled above");

    assert(
        param.shared_block->comms_type_ == CommsType::kReporting &&
        "Invalid comms type, a communicating process must be kReporting");

    if (-1 == fcntl(IpcCommsSync::sync_fd, F_SETFD, 0))
    {
        static_cast<void>(signal_safe_log_errno(errno, "fcntl at line ", __LINE__, " failed"));
        sysexit(EXIT_FAILURE);
    }
}

/// @details The implementation should be async signal safe.
void changeCurrentWorkingDirectory(const configuration::DeploymentConfig& config)
{
    // working_dir is set by configuration, so it should always be valid.
    // If not, chdir will fail anyway and we will log an error and exit.
    if (-1 == chdir(config.working_dir.c_str()))
    {
        static_cast<void>(signal_safe_log_errno(errno, "chdir(", config.working_dir, ") failed."));
        sysexit(EXIT_FAILURE);
    }
}

// req-Id: comp_req__launch_man__process_rlimit_support
/// @brief Applies every configured resource limit.
/// @details The implementation should be async signal safe. Must run before the potential privilege drop in
/// setSchedulingAndSecurity().
void applyResourceLimits(const configuration::Sandbox& config)
{
    for (const auto& rlimit_config : config.rlimits)
    {
        applyLimitOrDie(rlimit_config.resource, rlimit_config.limit, rlimitResourceName(rlimit_config.resource));
    }
}

/// @brief Sets up all the scheduling and security parameters described in the config, for the current process.
/// @details The implementation should be async signal safe.
/// @param config the configuration to use
/// @return kFail if any operation fails, kSuccess otherwise
OsalReturnType setSchedulingAndSecurity(const configuration::Sandbox& config)
{
    OsalReturnType retval = OsalReturnType::kSuccess;

    // Set process group id to be equal to the pid
    if (0 != setpgid(0, getpid()))
    {
        static_cast<void>(signal_safe_log_errno(errno, "setpgid() failed"));
        retval = OsalReturnType::kFail;
    }

    // Set scheduling policy with sched_setscheduler. The priority was clamped to the valid range of the policy
    // when the configuration was loaded.
    sched_param sch_param{};
    sch_param.sched_priority = config.scheduling_priority;

    if (-1 == sched_setscheduler(0, config.scheduling_policy, &sch_param))
    {
        static_cast<void>(signal_safe_log_errno(errno, "sched_setscheduler() failed"));
        retval = OsalReturnType::kFail;
    }

    // Set core affinity using OS specific functionality in osal
    if (config.affinity_mask.has_value())
    {
        if (-1 == setaffinity(config.affinity_mask.value()))
        {
            static_cast<void>(signal_safe_log_errno(errno, "setaffinity failed"));
            retval = OsalReturnType::kFail;
        }
    }

    // Set group ID
    if (-1 == setgid(config.gid))
    {
        static_cast<void>(signal_safe_log_errno(errno, "setgid(", config.gid, ") failed"));
        retval = OsalReturnType::kFail;
    }
    // Set supplementary group ids
    size_t supplementary_gids_number = config.supplementary_group_ids.size();

    // Note: the type of the first parameter of setgroups() differs in Linux and QNX, so we use osal
    if (-1 == setgroups(supplementary_gids_number, config.supplementary_group_ids.data()))
    {
        static_cast<void>(signal_safe_log_errno(errno, "setgroups() failed"));
        retval = OsalReturnType::kFail;
    }

    // Set user ID
    if (-1 == setuid(config.uid))
    {
        static_cast<void>(signal_safe_log_errno(errno, "setuid(", config.uid, ") failed"));
        retval = OsalReturnType::kFail;
    }

    return retval;
}

/// @brief Handles the execution of the child process after forking.
/// @details The implementation should be async signal safe.
/// @param[in] param Reference to child process configuration.
void handleChildProcess(ChildProcessConfig& param)
{
    handleComms(param);

    applyResourceLimits(param.config.deployment_config.sandbox);

    if (OsalReturnType::kSuccess != setSchedulingAndSecurity(param.config.deployment_config.sandbox))
    {
        sysexit(EXIT_FAILURE);
    }

    changeCurrentWorkingDirectory(param.config.deployment_config);

    // argv and envp are assembled when the configuration is loaded and are null-terminated
    char* const* argv = param.config.deployment_config.process_command.argv();
    char* const* envp = param.config.deployment_config.environmental_variables.envp();

    // Finally, execute the process, passing all the arguments and environment variables
    if (-1 == execve(argv[0], argv, envp))
    {
        static_cast<void>(signal_safe_log_errno(
            errno,
            "execve failed: Unable to execute the ",
            param.config.deployment_config.process_command.executablePath(),
            " app."));
        sysexit(EXIT_FAILURE);
    }
}

}  // namespace

OsalReturnType
startProcess(ProcessID& pid, const configuration::ComponentConfig& config, int comms_fd, const IpcCommsP& comms)
{
    const auto& security_policy = config.deployment_config.sandbox.security_policy;
    if (security_policy.has_value() && !security_policy.value().empty())
    {
        LM_LOG_WARN() << "Security policy" << security_policy.value() << "of" << config.name
                      << "is ignored, as security policies are not supported on Linux";
    }

    pid = fork();

    if (pid == 0)
    {
        /*
         * From this point on, only async signal safe functions can be
         * used. `fork` only copies the current thread, so any locks
         * which were held at that time will never be released.
         * See `man 2 fork`.
         */
        ChildProcessConfig param = {config, comms_fd, comms};
        handleChildProcess(param);
        // Only reached if execve was mocked in unit tests
        return OsalReturnType::kSuccess;
    }

    return pid > 0 ? OsalReturnType::kSuccess : OsalReturnType::kFail;
}

}  // namespace score::mw::lifecycle::internal::osal
