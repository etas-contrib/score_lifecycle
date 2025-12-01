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

#ifndef HEALTH_MONITOR_HPP_INCLUDED
#define HEALTH_MONITOR_HPP_INCLUDED
#include "score/lcm/saf/daemon/PhmDaemon.hpp"
namespace score
{
namespace lcm
{
namespace saf
{
namespace daemon
{

/// @brief Entry point for HM daemon thread
/// @param recovery_client Shared pointer to recovery client
/// @param init_status Initialization status to be updated
/// @param cancel_thread Termination signal predicate
void run(std::shared_ptr<score::lcm::RecoveryClient> recovery_client, std::atomic<EInitCode>& init_status, std::atomic_bool& cancel_thread);
}  // namespace daemon
}  // namespace saf
}  // namespace lcm
}  // namespace score
#endif