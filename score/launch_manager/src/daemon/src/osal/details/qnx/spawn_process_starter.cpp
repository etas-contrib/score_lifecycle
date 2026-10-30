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

#include <sched.h>
#include <secpol/secpol.h>
#include <spawn.h>
#include <sys/resource.h>
#include <signal.h>

#include "score/mw/launch_manager/common/log.hpp"
#include "score/mw/launch_manager/osal/details/qnx/spawn_process_starter.hpp"
#include "score/mw/launch_manager/osal/start_process.hpp"
#include "score/os/qnx/secpol_impl.h"
#include "score/os/qnx/unistd_impl.h"
#include "score/os/spawn_impl.h"
#include <cerrno>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

namespace score::mw::lifecycle::internal::osal
{

namespace
{

/// @brief Returns the description of @p error for log messages.
std::string_view errorMessage(const score::os::Error& error) noexcept
{
    return errno_message(error.GetOsDependentErrorCode());
}

/// @brief Owns the posix_spawn attribute and file action objects and the working directory fd.
class SpawnResources
{
  public:
    SpawnResources(const score::os::Spawn& spawn, const score::os::Unistd& unistd) noexcept
        : spawn_(spawn), unistd_(unistd)
    {
    }

    SpawnResources(const SpawnResources&) = delete;
    SpawnResources& operator=(const SpawnResources&) = delete;
    SpawnResources(SpawnResources&&) = delete;
    SpawnResources& operator=(SpawnResources&&) = delete;

    ~SpawnResources()
    {
        if (cwd_fd_ >= 0)
        {
            static_cast<void>(unistd_.close(cwd_fd_));
        }
        if (file_actions_initialized_)
        {
            static_cast<void>(spawn_.posix_spawn_file_actions_destroy(&file_actions_));
        }
        if (attr_initialized_)
        {
            static_cast<void>(spawn_.posix_spawnattr_destroy(&attr_));
        }
    }

    /// @return true on success, false otherwise (the reason is logged)
    bool init()
    {
        const auto attr_result = spawn_.posix_spawnattr_init(&attr_);
        if (!attr_result.has_value())
        {
            LM_LOG_ERROR() << "posix_spawnattr_init failed:" << errorMessage(attr_result.error());
            return false;
        }
        attr_initialized_ = true;

        const auto file_actions_result = spawn_.posix_spawn_file_actions_init(&file_actions_);
        if (!file_actions_result.has_value())
        {
            LM_LOG_ERROR() << "posix_spawn_file_actions_init failed:" << errorMessage(file_actions_result.error());
            return false;
        }
        file_actions_initialized_ = true;
        return true;
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
    const score::os::Spawn& spawn_;
    const score::os::Unistd& unistd_;
    posix_spawnattr_t attr_{};
    posix_spawn_file_actions_t file_actions_{};
    bool attr_initialized_{false};
    bool file_actions_initialized_{false};
    int cwd_fd_{-1};
};

/// @brief Fills the spawn attributes and file actions from the configuration.
/// @return true on success, false otherwise (the reason is logged)
bool prepareSpawn(
    const score::os::Spawn& spawn,
    const score::os::qnx::Secpol& secpol,
    const score::os::Fcntl& fcntl,
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
    const auto pgroup_result = spawn.posix_spawnattr_setpgroup(attr, 0);
    if (!pgroup_result.has_value())
    {
        LM_LOG_ERROR() << "posix_spawnattr_setpgroup failed:" << errorMessage(pgroup_result.error());
        return false;
    }
    flags |= POSIX_SPAWN_SETPGROUP;

    // The priority was clamped to the valid range of the policy when the configuration was loaded
    const auto policy_result = spawn.posix_spawnattr_setschedpolicy(attr, sandbox.scheduling_policy);
    if (!policy_result.has_value())
    {
        LM_LOG_ERROR() << "posix_spawnattr_setschedpolicy(" << sandbox.scheduling_policy
                       << ") failed:" << errorMessage(policy_result.error());
        return false;
    }
    sched_param sch_param{};
    sch_param.sched_priority = sandbox.scheduling_priority;
    const auto param_result = spawn.posix_spawnattr_setschedparam(attr, &sch_param);
    if (!param_result.has_value())
    {
        LM_LOG_ERROR() << "posix_spawnattr_setschedparam(" << sandbox.scheduling_priority
                       << ") failed:" << errorMessage(param_result.error());
        return false;
    }
    flags |= POSIX_SPAWN_SETSCHEDULER | POSIX_SPAWN_SETSCHEDPARAM;

    if (sandbox.affinity_mask.has_value())
    {
        const auto runmask_result = spawn.pthread_spawnattr_setrunmask_np(attr, sandbox.affinity_mask.value());
        if (!runmask_result.has_value())
        {
            LM_LOG_ERROR() << "pthread_spawnattr_setrunmask_np failed:" << errorMessage(runmask_result.error());
            return false;
        }
        flags |= POSIX_SPAWN_EXPLICIT_CPU;
    }

    const auto cred_result = spawn.posix_spawnattr_setcred(attr, sandbox.uid, sandbox.gid);
    if (!cred_result.has_value())
    {
        LM_LOG_ERROR() << "posix_spawnattr_setcred(" << sandbox.uid << "," << sandbox.gid
                       << ") failed:" << errorMessage(cred_result.error());
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
            const auto stackmax_result =
                spawn.posix_spawnattr_setstackmax(attr, static_cast<std::uint32_t>(rlimit_config.limit.rlim_cur));
            if (!stackmax_result.has_value())
            {
                LM_LOG_ERROR() << "posix_spawnattr_setstackmax failed:" << errorMessage(stackmax_result.error());
                return false;
            }
            flags |= POSIX_SPAWN_SETSTACKMAX;
        }
    }

    // working_dir is set by configuration, so it should always be valid.
    const auto cwd_result = fcntl.open(
        config.deployment_config.working_dir.c_str(),
        score::os::Fcntl::Open::kReadOnly | score::os::Fcntl::Open::kDirectory | score::os::Fcntl::Open::kCloseOnExec);
    if (!cwd_result.has_value())
    {
        LM_LOG_ERROR() << "open(" << config.deployment_config.working_dir
                       << ") failed:" << errorMessage(cwd_result.error());
        return false;
    }
    resources.setCwdFd(cwd_result.value());
    const auto setcwd_result = spawn.posix_spawnattr_setcwd_np(attr, cwd_result.value());
    if (!setcwd_result.has_value())
    {
        LM_LOG_ERROR() << "posix_spawnattr_setcwd_np(" << config.deployment_config.working_dir
                       << ") failed:" << errorMessage(setcwd_result.error());
        return false;
    }
    flags |= POSIX_SPAWN_SETCWD;

    if (sandbox.security_policy.has_value() && !sandbox.security_policy.value().empty())
    {
        // The configured name refers directly to a type name
        const auto typeid_result = secpol.secpol_posix_spawnattr_settypeid(
            nullptr, attr, sandbox.security_policy.value().c_str(), SECPOL_TYPE_NAME);
        if (!typeid_result.has_value() && typeid_result.error() != score::os::Error::Code::kOperationNotSupported)
        {
            LM_LOG_ERROR() << "secpol_posix_spawnattr_settypeid(" << sandbox.security_policy.value()
                           << ") failed:" << errorMessage(typeid_result.error());
            return false;
        }
    }

    // Get, OR and set the flags, so that flags set by the functions above (e.g. POSIX_SPAWN_SETTYPEID) are kept
    std::uint32_t current_flags = 0U;
    const auto getxflags_result = spawn.posix_spawnattr_getxflags(attr, &current_flags);
    if (!getxflags_result.has_value())
    {
        LM_LOG_ERROR() << "posix_spawnattr_getxflags failed:" << errorMessage(getxflags_result.error());
        return false;
    }
    const auto setxflags_result = spawn.posix_spawnattr_setxflags(attr, current_flags | flags);
    if (!setxflags_result.has_value())
    {
        LM_LOG_ERROR() << "posix_spawnattr_setxflags failed:" << errorMessage(setxflags_result.error());
        return false;
    }

    if (comms)
    {
        // The duplicated fd does not have FD_CLOEXEC set, so it remains open in the new process
        // Note that comms_fd == IpcCommsSync::sync_fd cannot happen as IpcCommsSync::sync_fd is being reserved during
        // startup
        const auto dup2_result =
            spawn.posix_spawn_file_actions_adddup2(resources.fileActions(), comms_fd, IpcCommsSync::sync_fd);
        if (!dup2_result.has_value())
        {
            LM_LOG_ERROR() << "posix_spawn_file_actions_adddup2 failed:" << errorMessage(dup2_result.error());
            return false;
        }
    }

    return true;
}

// req-Id: comp_req__launch_man__process_rlimit_support
/// @brief Applies the supplementary groups and resource limits to the held process @p pid.
/// @return true on success, false otherwise (the reason is logged)
bool configureHeldProcess(
    const score::os::qnx::QnxUnistd& qnx_unistd,
    ProcessID pid,
    const configuration::Sandbox& sandbox)
{
    // Always set the supplementary groups, so the groups of the Launch Manager are not inherited
    const auto supplementary_gids_number = static_cast<std::int32_t>(sandbox.supplementary_group_ids.size());
    const auto groups_result = qnx_unistd.setgroupspid(
        supplementary_gids_number,
        supplementary_gids_number != 0 ? sandbox.supplementary_group_ids.data() : nullptr,
        pid);
    if (!groups_result.has_value())
    {
        LM_LOG_ERROR() << "setgroupspid() failed for pid" << pid << ":" << errorMessage(groups_result.error());
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

SpawnProcessStarter::SpawnProcessStarter(
    std::unique_ptr<score::os::Spawn> spawn,
    std::unique_ptr<score::os::qnx::Secpol> secpol,
    std::unique_ptr<score::os::qnx::QnxUnistd> qnx_unistd,
    std::unique_ptr<score::os::Fcntl> fcntl,
    std::unique_ptr<score::os::Unistd> unistd) noexcept
    : spawn_(std::move(spawn)),
      secpol_(std::move(secpol)),
      qnx_unistd_(std::move(qnx_unistd)),
      fcntl_(std::move(fcntl)),
      unistd_(std::move(unistd))
{
}

OsalReturnType SpawnProcessStarter::startProcess(
    ProcessID& pid,
    const configuration::ComponentConfig& config,
    int comms_fd,
    const IpcCommsP& comms) const
{
    pid = -1;

    SpawnResources resources{*spawn_, *unistd_};
    if (!resources.init())
    {
        return OsalReturnType::kFail;
    }

    if (!prepareSpawn(*spawn_, *secpol_, *fcntl_, resources, config, comms_fd, comms))
    {
        return OsalReturnType::kFail;
    }

    // argv and envp are assembled when the configuration is loaded and are null-terminated
    char* const* argv = config.deployment_config.process_command.argv();
    char* const* envp = config.deployment_config.environmental_variables.envp();

    ProcessID child_pid = -1;
    const auto spawn_result =
        spawn_->posix_spawn(&child_pid, argv[0], resources.fileActions(), resources.attr(), argv, envp);
    if (!spawn_result.has_value())
    {
        LM_LOG_ERROR() << "posix_spawn failed for" << config.deployment_config.process_command.executablePath()
                       << "with error:" << errorMessage(spawn_result.error());
        return OsalReturnType::kFail;
    }
    pid = child_pid;

    // The process is held, so storing the pid cannot race with the check at the client end
    if (comms)
    {
        comms->pid_ = pid;
    }

    if (!configureHeldProcess(*qnx_unistd_, pid, config.deployment_config.sandbox))
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

std::unique_ptr<IProcessStarter> createProcessStarter()
{
    return std::make_unique<SpawnProcessStarter>(
        std::make_unique<score::os::SpawnImpl>(),
        std::make_unique<score::os::qnx::details::SecpolImpl>(),
        std::make_unique<score::os::qnx::QnxUnistdImpl>(),
        score::os::Fcntl::Default(),
        score::os::Unistd::Default());
}

}  // namespace score::mw::lifecycle::internal::osal
