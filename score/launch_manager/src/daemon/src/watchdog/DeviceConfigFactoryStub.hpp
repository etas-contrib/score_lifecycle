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

#ifndef DEVICECONFIGFACTORYSTUB_HPP_INCLUDED
#define DEVICECONFIGFACTORYSTUB_HPP_INCLUDED

#include "score/mw/launch_manager/watchdog/IDeviceConfigFactory.hpp"

namespace score
{
namespace lcm
{
namespace watchdog
{

/// @brief Trivial stub for IDeviceConfigFactory, for use by tests that need to inject a valid, non-null factory
/// (e.g. into IWatchdogIf::init()) without asserting on it.
class DeviceConfigFactoryStub final : public IDeviceConfigFactory
{
  public:
    DeviceConfigFactoryStub() = default;

    std::optional<DeviceConfigurations> getDeviceConfigurations() const override
    {
        return DeviceConfigurations{};
    }
};

}  // namespace watchdog
}  // namespace lcm
}  // namespace score

#endif  // DEVICECONFIGFACTORYSTUB_HPP_INCLUDED
