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

#ifndef OSAL_DETAILS_LINUX_FORK_PROCESS_STARTER_HPP_INCLUDED
#define OSAL_DETAILS_LINUX_FORK_PROCESS_STARTER_HPP_INCLUDED

#include "score/mw/launch_manager/osal/iprocess_starter.hpp"

namespace score::mw::lifecycle::internal::osal
{

/// @brief IProcessStarter implementation using fork() and execve().
/// @details The sandbox is applied in the child process between fork() and execve().
class ForkProcessStarter final : public IProcessStarter
{
  public:
    /// @see IProcessStarter::startProcess() for details
    OsalReturnType startProcess(
        ProcessID& pid,
        const configuration::ComponentConfig& config,
        int comms_fd,
        const IpcCommsP& comms) const override;
};

}  // namespace score::mw::lifecycle::internal::osal

#endif  // OSAL_DETAILS_LINUX_FORK_PROCESS_STARTER_HPP_INCLUDED
