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

#ifndef OSAL_DETAILS_QNX_SPAWN_PROCESS_STARTER_HPP_INCLUDED
#define OSAL_DETAILS_QNX_SPAWN_PROCESS_STARTER_HPP_INCLUDED

#include "score/mw/launch_manager/osal/iprocess_starter.hpp"
#include "score/os/fcntl.h"
#include "score/os/qnx/secpol.h"
#include "score/os/qnx/unistd.h"
#include "score/os/spawn.h"
#include "score/os/unistd.h"

#include <memory>

namespace score::mw::lifecycle::internal::osal
{

/// @brief IProcessStarter implementation using posix_spawn() and its QNX extensions.
/// @details The process is spawned held, so the supplementary groups and resource limits can be applied before it
/// runs.
class SpawnProcessStarter final : public IProcessStarter
{
  public:
    /// @param spawn Used to set up the spawn attributes and to spawn the process
    /// @param secpol Used to apply the security policy
    /// @param qnx_unistd Used to set the supplementary groups of the held process
    /// @param fcntl Used to open the working directory
    /// @param unistd Used to close the working directory
    SpawnProcessStarter(
        std::unique_ptr<score::os::Spawn> spawn,
        std::unique_ptr<score::os::qnx::Secpol> secpol,
        std::unique_ptr<score::os::qnx::QnxUnistd> qnx_unistd,
        std::unique_ptr<score::os::Fcntl> fcntl,
        std::unique_ptr<score::os::Unistd> unistd) noexcept;

    /// @see IProcessStarter::startProcess() for details
    OsalReturnType startProcess(
        ProcessID& pid,
        const configuration::ComponentConfig& config,
        int comms_fd,
        const IpcCommsP& comms) const override;

  private:
    std::unique_ptr<score::os::Spawn> spawn_;
    std::unique_ptr<score::os::qnx::Secpol> secpol_;
    std::unique_ptr<score::os::qnx::QnxUnistd> qnx_unistd_;
    std::unique_ptr<score::os::Fcntl> fcntl_;
    std::unique_ptr<score::os::Unistd> unistd_;
};

}  // namespace score::mw::lifecycle::internal::osal

#endif  // OSAL_DETAILS_QNX_SPAWN_PROCESS_STARTER_HPP_INCLUDED
