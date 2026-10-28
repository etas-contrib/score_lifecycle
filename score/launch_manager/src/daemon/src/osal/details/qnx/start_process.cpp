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

#include <fcntl.h>
#include <sched.h>
#include <secpol/secpol.h>
#include <spawn.h>
#include <sys/resource.h>
#include <unistd.h>
#include <signal.h>

#include "score/mw/launch_manager/common/log.hpp"
#include "score/mw/launch_manager/osal/start_process.hpp"
#include <cerrno>
#include <cstdint>
#include <limits>

namespace score::mw::lifecycle::internal::osal
{

namespace
{

/// @brief Owns the posix_spawn attribute and file action objects and the working directory fd.
class SpawnResources
{
  public:
    SpawnResources() = default;

    SpawnResources(const SpawnResources&) = delete;
    SpawnResources& operator=(const SpawnResources&) = delete;
    SpawnResources(SpawnResources&&) = delete;
    SpawnResources& operator=(SpawnResources&&) = delete;

    ~SpawnResources()
    {
        if (cwd_fd_ >= 0)
        {
            static_cast<void>(::close(cwd_fd_));
        }
        if (file_actions_initialized_)
        {
            static_cast<void>(::posix_spawn_file_actions_destroy(&file_actions_));
        }
        if (attr_initialized_)
        {
            static_cast<void>(::posix_spawnattr_destroy(&attr_));
        }
    }

    /// @return 0 on success, an error number otherwise
    int init()
    {
        int rc = ::posix_spawnattr_init(&attr_);
        if (rc != 0)
        {
            return rc;
        }
        attr_initialized_ = true;

        rc = ::posix_spawn_file_actions_init(&file_actions_);
        if (rc != 0)
        {
            return rc;
        }
        file_actions_initialized_ = true;
        return 0;
    }

    posix_spawnattr_t* attr()
    {
        return &attr_;
    }

    posix_spawn_file_actions_t* fileActions()
    {
        return &file_actions_;
    }

    void setCwdFd(int fd)
    {
        cwd_fd_ = fd;
    }

  private:
    posix_spawnattr_t attr_{};
    posix_spawn_file_actions_t file_actions_{};
    bool attr_initialized_{false};
    bool file_actions_initialized_{false};
    int cwd_fd_{-1};
};

/// @brief Fills the spawn attributes and file actions from the configuration.
/// @return true on success, false otherwise (the reason is logged)
bool prepareSpawn(
    SpawnResources& resources,
    const configuration::ComponentConfig& config,
    int comms_fd,
    const IpcCommsP& comms)
{
    const auto& sandbox = config.deployment_config.sandbox;
    posix_spawnattr_t* attr = resources.attr();

    // Start the process held, so the supplementary groups and resource limits can be applied before it runs
    std::uint32_t flags = POSIX_SPAWN_HOLD;

    // Set process group id to be equal to the pid
    int rc = ::posix_spawnattr_setpgroup(attr, 0);
    if (rc != 0)
    {
        LM_LOG_ERROR() << "posix_spawnattr_setpgroup failed:" << errno_message(rc);
        return false;
    }
    flags |= POSIX_SPAWN_SETPGROUP;

    // The priority was clamped to the valid range of the policy when the configuration was loaded
    rc = ::posix_spawnattr_setschedpolicy(attr, sandbox.scheduling_policy);
    if (rc != 0)
    {
        LM_LOG_ERROR() << "posix_spawnattr_setschedpolicy(" << sandbox.scheduling_policy
                       << ") failed:" << errno_message(rc);
        return false;
    }
    sched_param sch_param{};
    sch_param.sched_priority = sandbox.scheduling_priority;
    rc = ::posix_spawnattr_setschedparam(attr, &sch_param);
    if (rc != 0)
    {
        LM_LOG_ERROR() << "posix_spawnattr_setschedparam(" << sandbox.scheduling_priority
                       << ") failed:" << errno_message(rc);
        return false;
    }
    flags |= POSIX_SPAWN_SETSCHEDULER | POSIX_SPAWN_SETSCHEDPARAM;

    if (sandbox.affinity_mask.has_value())
    {
        rc = ::pthread_spawnattr_setrunmask_np(attr, sandbox.affinity_mask.value());
        if (rc != 0)
        {
            LM_LOG_ERROR() << "pthread_spawnattr_setrunmask_np failed:" << errno_message(rc);
            return false;
        }
        flags |= POSIX_SPAWN_EXPLICIT_CPU;
    }

    rc = ::posix_spawnattr_setcred(attr, sandbox.uid, sandbox.gid);
    if (rc != 0)
    {
        LM_LOG_ERROR() << "posix_spawnattr_setcred(" << sandbox.uid << "," << sandbox.gid
                       << ") failed:" << errno_message(rc);
        return false;
    }
    flags |= POSIX_SPAWN_SETCRED;

    // The stack of the main thread already exists while the process is held, so a stack limit applied afterwards
    // with prlimit() would not affect it. Set the maximum stack size via the spawn attributes as well.
    for (const auto& rlimit_config : sandbox.rlimits)
    {
        if (rlimit_config.resource == RLIMIT_STACK && rlimit_config.limit.rlim_cur != RLIM_INFINITY &&
            rlimit_config.limit.rlim_cur <= std::numeric_limits<std::uint32_t>::max())
        {
            rc = ::posix_spawnattr_setstackmax(attr, static_cast<std::uint32_t>(rlimit_config.limit.rlim_cur));
            if (rc != 0)
            {
                LM_LOG_ERROR() << "posix_spawnattr_setstackmax failed:" << errno_message(rc);
                return false;
            }
            flags |= POSIX_SPAWN_SETSTACKMAX;
        }
    }

    // working_dir is set by configuration, so it should always be valid.
    const int cwd_fd = ::open(config.deployment_config.working_dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (cwd_fd < 0)
    {
        LM_LOG_ERROR() << "open(" << config.deployment_config.working_dir << ") failed:" << errno_message(errno);
        return false;
    }
    resources.setCwdFd(cwd_fd);
    rc = ::posix_spawnattr_setcwd_np(attr, cwd_fd);
    if (rc != 0)
    {
        LM_LOG_ERROR() << "posix_spawnattr_setcwd_np(" << config.deployment_config.working_dir
                       << ") failed:" << errno_message(rc);
        return false;
    }
    flags |= POSIX_SPAWN_SETCWD;

    if (sandbox.security_policy.has_value() && !sandbox.security_policy.value().empty())
    {
        // The configured name refers directly to a type name
        if (::secpol_posix_spawnattr_settypeid(
                nullptr, attr, sandbox.security_policy.value().c_str(), SECPOL_TYPE_NAME) != 0 &&
            errno != ENOTSUP)
        {
            LM_LOG_ERROR() << "secpol_posix_spawnattr_settypeid(" << sandbox.security_policy.value()
                           << ") failed:" << errno_message(errno);
            return false;
        }
    }

    // Get, OR and set the flags, so that flags set by the functions above (e.g. POSIX_SPAWN_SETTYPEID) are kept
    std::uint32_t current_flags = 0U;
    rc = ::posix_spawnattr_getxflags(attr, &current_flags);
    if (rc == 0)
    {
        rc = ::posix_spawnattr_setxflags(attr, current_flags | flags);
    }
    if (rc != 0)
    {
        LM_LOG_ERROR() << "Setting the posix_spawn flags failed:" << errno_message(rc);
        return false;
    }

    if (comms)
    {
        // The duplicated fd does not have FD_CLOEXEC set, so it remains open in the new process
        rc = ::posix_spawn_file_actions_adddup2(resources.fileActions(), comms_fd, IpcCommsSync::sync_fd);
        if (rc != 0)
        {
            LM_LOG_ERROR() << "posix_spawn_file_actions_adddup2 failed:" << errno_message(rc);
            return false;
        }
    }

    return true;
}

// req-Id: comp_req__launch_man__process_rlimit_support
/// @brief Applies the supplementary groups and resource limits to the held process @p pid.
/// @return true on success, false otherwise (the reason is logged)
bool configureHeldProcess(ProcessID pid, const configuration::Sandbox& sandbox)
{
    // Always set the supplementary groups, so the groups of the Launch Manager are not inherited
    const auto supplementary_gids_number = static_cast<int>(sandbox.supplementary_group_ids.size());
    if (-1 == ::setgroupspid(
                  supplementary_gids_number,
                  supplementary_gids_number != 0 ? sandbox.supplementary_group_ids.data() : nullptr,
                  pid))
    {
        LM_LOG_ERROR() << "setgroupspid() failed for pid" << pid << ":" << errno_message(errno);
        return false;
    }

    for (const auto& rlimit_config : sandbox.rlimits)
    {
        if (-1 == ::prlimit(pid, rlimit_config.resource, &rlimit_config.limit, nullptr))
        {
            LM_LOG_ERROR() << "prlimit(" << rlimit_config.resource << ") failed for pid" << pid << ":"
                           << errno_message(errno);
            return false;
        }
    }

    return true;
}

}  // namespace

OsalReturnType
startProcess(ProcessID& pid, const configuration::ComponentConfig& config, int comms_fd, const IpcCommsP& comms)
{
    pid = -1;

    SpawnResources resources;
    const int init_rc = resources.init();
    if (init_rc != 0)
    {
        LM_LOG_ERROR() << "Initializing the posix_spawn attributes failed:" << errno_message(init_rc);
        return OsalReturnType::kFail;
    }

    if (!prepareSpawn(resources, config, comms_fd, comms))
    {
        return OsalReturnType::kFail;
    }

    // argv and envp are assembled when the configuration is loaded and are null-terminated
    char* const* argv = config.deployment_config.process_command.argv();
    char* const* envp = config.deployment_config.environmental_variables.envp();

    ProcessID child_pid = -1;
    const int spawn_rc = ::posix_spawn(&child_pid, argv[0], resources.fileActions(), resources.attr(), argv, envp);
    if (spawn_rc != 0)
    {
        LM_LOG_ERROR() << "posix_spawn failed for" << config.deployment_config.process_command.executablePath()
                       << "with error:" << errno_message(spawn_rc);
        return OsalReturnType::kFail;
    }
    pid = child_pid;

    // The process is held, so storing the pid cannot race with the check at the client end
    if (comms)
    {
        comms->pid_ = pid;
    }

    if (!configureHeldProcess(pid, config.deployment_config.sandbox))
    {
        // The process exists, so report it as started and let it terminate. This way the termination is reported
        // via the normal path, like a forked process that fails to apply its configuration on Linux.
        if (-1 == ::kill(pid, SIGKILL))
        {
            LM_LOG_ERROR() << "Unable to kill held process" << pid << ":" << errno_message(errno);
        }
        return OsalReturnType::kSuccess;
    }

    if (-1 == ::kill(pid, SIGCONT))
    {
        LM_LOG_ERROR() << "Unable to continue held process" << pid << ":" << errno_message(errno);
        static_cast<void>(::kill(pid, SIGKILL));
    }

    return OsalReturnType::kSuccess;
}

}  // namespace score::mw::lifecycle::internal::osal
