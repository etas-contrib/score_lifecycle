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

#include "score/mw/launch_manager/watchdog/default/default_watchdog.hpp"

#include "score/mw/launch_manager/common/log.hpp"
#include "score/mw/launch_manager/watchdog/watchdog_factory.hpp"

#include "score/memory/shared/shared_memory_factory.h"

#include <cstdlib>
#include <utility>

#ifdef LM_DEFAULT_WATCHDOG_REACTION_REBOOT
#include <cerrno>
#ifdef __QNXNTO__
#include <sys/sysmgr.h>
#else
#include <sys/reboot.h>
#include <unistd.h>
#endif
#endif

namespace
{

#ifdef LM_DEFAULT_WATCHDOG_REACTION_REBOOT
void rebootMachine()
{
#ifdef __QNXNTO__
    static_cast<void>(sysmgr_reboot());
#else
    sync();
    static_cast<void>(::reboot(RB_AUTOBOOT));
#endif
}
#endif

}  // namespace

namespace score::mw::lifecycle::internal::watchdog
{

using score::memory::shared::ISharedMemoryResource;
using score::memory::shared::SharedMemoryFactory;

DefaultWatchdog::DefaultWatchdog(std::string heartbeat_path) : heartbeat_path_(std::move(heartbeat_path))
{
}

bool DefaultWatchdog::init() noexcept
{
    // A previous Launch Manager run that did not shut down orderly leaves the region behind.
    SharedMemoryFactory::RemoveStaleArtefacts(heartbeat_path_);

    // WorldReadable: only the Launch Manager user can write, everybody else can only read.
    heartbeat_resource_ = SharedMemoryFactory::Create(
        heartbeat_path_,
        [](std::shared_ptr<ISharedMemoryResource> resource) {
            static_cast<void>(resource->construct<Heartbeat>());
        },
        sizeof(Heartbeat),
        SharedMemoryFactory::WorldReadable{});
    if (heartbeat_resource_ == nullptr)
    {
        LM_LOG_ERROR() << "Watchdog: failed to create heartbeat shared memory" << heartbeat_path_;
        return false;
    }

    // The heartbeat is the first and only object in the region.
    heartbeat_ = static_cast<Heartbeat*>(heartbeat_resource_->getUsableBaseAddress());
    return true;
}

void DefaultWatchdog::serviceWatchdog() noexcept
{
    if (heartbeat_ != nullptr)
    {
        static_cast<void>(heartbeat_->counter.fetch_add(1U, std::memory_order_relaxed));
    }
}

void DefaultWatchdog::fireWatchdog() noexcept
{
    // Stop the heartbeat, in case the reaction below does not terminate the Launch Manager immediately.
    heartbeat_ = nullptr;
#ifdef LM_DEFAULT_WATCHDOG_REACTION_REBOOT
    LM_LOG_FATAL() << "Watchdog: fired, rebooting the machine";
    rebootMachine();
    // Only reached if the reboot failed. The Launch Manager must not continue running.
    LM_LOG_FATAL() << "Watchdog: reboot failed:" << errno_message(errno);
#else
    LM_LOG_FATAL() << "Watchdog: fired, aborting the Launch Manager";
#endif
    std::abort();
}

void DefaultWatchdog::deinit() noexcept
{
    heartbeat_ = nullptr;
    if (heartbeat_resource_ != nullptr)
    {
        // Remove() only finds the resource while it is alive, so it must be called before releasing it.
        SharedMemoryFactory::Remove(heartbeat_path_);
        heartbeat_resource_.reset();
    }
}

std::unique_ptr<IWatchdog> createWatchdog(int /*argc*/, const char* /*argv*/[])
{
    return std::make_unique<DefaultWatchdog>();
}

}  // namespace score::mw::lifecycle::internal::watchdog
