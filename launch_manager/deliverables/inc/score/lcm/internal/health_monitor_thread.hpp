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


#ifndef HEALTH_MONITOR_THREAD_HPP_INCLUDED
#define HEALTH_MONITOR_THREAD_HPP_INCLUDED

#include <thread>
#include <atomic>
#include <score/lcm/recovery_client.h>

namespace score
{
namespace lcm
{
namespace internal
{

/// @brief HealthMonitorThread manages the lifecycle of the Health Monitor daemon in a separate thread.
class HealthMonitorThread final {
 public:
    /// @brief Starts the Health Monitor thread.
    /// @param client Shared pointer to the RecoveryClient used for communication with Launch Manager.
    /// @return true if the Health Monitor started successfully, false otherwise.
    bool start(std::shared_ptr<score::lcm::RecoveryClient> client) noexcept;

    /// @brief Stops the Health Monitor thread.
    void stop() noexcept;
 private:
    std::thread health_monitor_thread_{};
    std::atomic_bool stop_thread_{false};
};

    
} 
}
}
#endif