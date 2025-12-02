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


#include <score/lcm/internal/health_monitor.hpp>
#include <score/lcm/saf/daemon/health_monitor.hpp>

namespace score
{
namespace lcm
{
namespace internal 
{
bool HealthMonitor::start(std::shared_ptr<score::lcm::IRecoveryClient> client) noexcept {
    std::atomic<score::lcm::saf::daemon::EInitCode> init_status{score::lcm::saf::daemon::EInitCode::kNotInitialized};
    health_monitor_thread_ = std::thread(&score::lcm::saf::daemon::run, client, std::ref(init_status), std::ref(stop_thread_));
    
    std::unique_lock lk(score::lcm::saf::daemon::initialization_mutex);
    score::lcm::saf::daemon::initialization_cv.wait(
        lk,
        [&init_status]() {
            return init_status.load() != score::lcm::saf::daemon::EInitCode::kNotInitialized;
        });

    return (init_status.load() == score::lcm::saf::daemon::EInitCode::kNoError);
}

void HealthMonitor::stop() noexcept {
    stop_thread_.store(true);
    if (health_monitor_thread_.joinable()) {
        health_monitor_thread_.join();
    }
}
}
}
}
