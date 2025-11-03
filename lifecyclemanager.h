// *******************************************************************************
// Copyright (c) 2025 Contributors to the Eclipse Foundation
//
// See the NOTICE file(s) distributed with this work for additional
// information regarding copyright ownership.
//
// This program and the accompanying materials are made available under the
// terms of the Apache License Version 2.0 which is available at
// https://www.apache.org/licenses/LICENSE-2.0
//
// SPDX-License-Identifier: Apache-2.0
// *******************************************************************************

#ifndef PLATFORM_AAS_MW_LIFECYCLE_LIFECYCLEMANAGER_H_
#define PLATFORM_AAS_MW_LIFECYCLE_LIFECYCLEMANAGER_H_

#include "platform/aas/mw/lifecycle/application.h"
#include "platform/aas/mw/lifecycle/applicationcontext.h"

#include "platform/aas/lib/os/utils/signal_impl.h"

#include "amp_stop_token.hpp"

#include <cstdint>
#include <thread>

namespace bmw
{
namespace mw
{
namespace lifecycle
{

/**
 * \brief Implements life cycle of an Application.
 *
 * Protected methods shall be implemented with logic
 * which is stack specific.
 */
class LifeCycleManager
{
  private:
    /**
     * \brief stop_source object used as sync and cancel mechanism for application
     */
    amp::stop_source m_stop_source;

    /**
     * \brief Runnable application
     */
    Application* m_app;

    /**
     * \brief signal handling mask.
     */
    sigset_t m_signal_set;

    /**
     * \brief The thread dedicated to signal handling.
     */
    std::thread m_signal_handler_thread; /* NOLINT(bmw-banned-type) using std::thread by desing */

    std::unique_ptr<bmw::os::Signal> signal_;

    /**
     * \brief Sets up signal handling and spawns m_signal_handler_thread.
     */
    bool initialize_internal();

    /**
     * \brief Signal handler to catch signals sent to the Application.
     *
     * This signal handler only handles SIGTERM, which asks the application to terminate.
     */
    void handle_signal();

    /**
     * \brief Clean up function that should be called on exit.
     */
    void at_exit() noexcept;

  protected:
    /**
     * \brief Hook function for reporting running state.
     */
    virtual void report_running() noexcept {};
    /**
     * \brief Hook function for reporting shutdown state.
     */
    virtual void report_shutdown() noexcept {};

  public:
    explicit LifeCycleManager(
        std::unique_ptr<bmw::os::Signal> signal_interface = std::make_unique<bmw::os::SignalImpl>()) noexcept;
    virtual ~LifeCycleManager() noexcept;

    /*
     * Ensure that objects of this type are not copyable and not movable.
     */
    LifeCycleManager(LifeCycleManager&& other) = delete;
    LifeCycleManager(const LifeCycleManager& other) = delete;
    LifeCycleManager& operator=(LifeCycleManager&& other) = delete;
    LifeCycleManager& operator=(const LifeCycleManager& other) = delete;

    /**
     * \brief Function dedicated to run application.
     */
    std::int32_t run(Application& app, const ApplicationContext&);
};

}  // namespace lifecycle
}  // namespace mw
}  // namespace bmw

#endif  // PLATFORM_AAS_MW_LIFECYCLE_LIFECYCLEMANAGER_H_
