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

#ifndef OSAL_MOCK_IPROCESS_STARTER_HPP_INCLUDED
#define OSAL_MOCK_IPROCESS_STARTER_HPP_INCLUDED

#include "score/mw/launch_manager/osal/iprocess_starter.hpp"
#include <gmock/gmock.h>

namespace score::mw::lifecycle::internal::osal
{

class MockIProcessStarter : public IProcessStarter
{
  public:
    MOCK_METHOD(
        OsalReturnType,
        startProcess,
        (ProcessID & pid, const configuration::ComponentConfig& config, int comms_fd, const IpcCommsP& comms),
        (const, override));
};

}  // namespace score::mw::lifecycle::internal::osal

#endif  // OSAL_MOCK_IPROCESS_STARTER_HPP_INCLUDED
