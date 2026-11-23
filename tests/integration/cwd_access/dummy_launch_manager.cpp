/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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

// Minimal launch manager: prepares a directory tree that is only accessible to root, then starts the process given
// as first argument via osal::startProcess() as an unprivileged user, with its working directory inside that tree.
// Exits with EXIT_SUCCESS only if the started process exits with EXIT_SUCCESS.

#include "score/mw/launch_manager/configuration/component_config.hpp"
#include "score/mw/launch_manager/configuration/process_command.hpp"
#include "score/mw/launch_manager/osal/ipc_comms.hpp"
#include "score/mw/launch_manager/osal/return_types.hpp"
#include "score/mw/launch_manager/osal/start_process.hpp"

#include <sched.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{

namespace configuration = score::mw::lifecycle::internal::configuration;
namespace osal = score::mw::lifecycle::internal::osal;

constexpr const char* kLockedDir = "/tmp/locked";
constexpr const char* kVisibleDir = "/tmp/locked/visible";
constexpr const char* kSecretFile = "/tmp/locked/visible/secret.txt";
constexpr uid_t kUnprivilegedUid = 1000;
constexpr gid_t kUnprivilegedGid = 1000;

bool changeMode(const char* path, mode_t mode)
{
    if (::chmod(path, mode) != 0)
    {
        std::cerr << "chmod(" << path << ") failed: " << std::strerror(errno) << std::endl;
        return false;
    }
    return true;
}

/// @brief Creates /tmp/locked (root only) containing visible/secret.txt (accessible to everyone).
bool setupLockedDirectory()
{
    std::error_code error;
    std::filesystem::remove_all(kLockedDir, error);
    if (!std::filesystem::create_directories(kVisibleDir, error) || error)
    {
        std::cerr << "Cannot create " << kVisibleDir << ": " << error.message() << std::endl;
        return false;
    }

    {
        std::ofstream secret{kSecretFile};
        secret << "Pa$$w0rd\n";
        if (!secret)
        {
            std::cerr << "Cannot write " << kSecretFile << std::endl;
            return false;
        }
    }

    // /tmp/locked is only accessible to root, but its children are accessible to everyone
    return changeMode(kLockedDir, 0700) && changeMode(kVisibleDir, 0755) && changeMode(kSecretFile, 0644);
}

void cleanupLockedDirectory()
{
    std::error_code error;
    std::filesystem::remove_all(kLockedDir, error);
}

/// @brief Builds the configuration of the process at @p executable, started unprivileged in kVisibleDir.
bool makeConfig(configuration::ComponentConfig& config, const std::string& executable)
{
    config.name = "cwd_access_process";
    config.deployment_config.process_command = configuration::ProcessCommand{executable, {}};
    config.deployment_config.working_dir = kVisibleDir;

    auto& sandbox = config.deployment_config.sandbox;
    sandbox.uid = kUnprivilegedUid;
    sandbox.gid = kUnprivilegedGid;

    // Use the own scheduling, which is guaranteed to be valid
    const int policy = ::sched_getscheduler(0);
    sched_param param{};
    if (policy < 0 || ::sched_getparam(0, &param) != 0)
    {
        std::cerr << "Cannot get the scheduling parameters: " << std::strerror(errno) << std::endl;
        return false;
    }
    sandbox.scheduling_policy = policy;
    sandbox.scheduling_priority = param.sched_priority;
    return true;
}

/// @brief Starts the process described by @p config and waits for it to terminate.
/// @return true if the process exited with EXIT_SUCCESS
bool runProcess(const configuration::ComponentConfig& config)
{
    osal::ProcessID pid = -1;
    if (osal::startProcess(pid, config, -1, osal::IpcCommsP{}) != osal::OsalReturnType::kSuccess)
    {
        std::cerr << "startProcess() failed" << std::endl;
        return false;
    }

    int status = 0;
    pid_t result = -1;
    do
    {
        result = ::waitpid(pid, &status, 0);
    } while (result < 0 && errno == EINTR);
    if (result != pid)
    {
        std::cerr << "waitpid() failed: " << std::strerror(errno) << std::endl;
        return false;
    }

    if (WIFEXITED(status))
    {
        std::cout << "Child exited with code " << WEXITSTATUS(status) << std::endl;
        return WEXITSTATUS(status) == EXIT_SUCCESS;
    }
    if (WIFSIGNALED(status))
    {
        std::cerr << "Child was terminated by signal " << WTERMSIG(status) << std::endl;
    }
    return false;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <path of the process to start>" << std::endl;
        return EXIT_FAILURE;
    }
    if (::geteuid() != 0)
    {
        std::cerr << argv[0] << " must run as root" << std::endl;
        return EXIT_FAILURE;
    }

    if (!setupLockedDirectory())
    {
        cleanupLockedDirectory();
        return EXIT_FAILURE;
    }

    configuration::ComponentConfig config{};
    const bool passed = makeConfig(config, argv[1]) && runProcess(config);

    cleanupLockedDirectory();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
