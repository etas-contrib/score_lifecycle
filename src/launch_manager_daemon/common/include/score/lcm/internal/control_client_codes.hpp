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

#ifndef CONTROL_CLIENT_CODES_HPP_INCLUDED
#define CONTROL_CLIENT_CODES_HPP_INCLUDED

#include <cstdint>

namespace score {

namespace lcm {

namespace internal {

/// @brief Code for requests from Control Client and responses back from Launch Manager
enum class ControlClientCode
{
    // General
    kNotSet = 0,
    kInvalidRequest = 1,

    // setState functionality
    kSetStateRequest = 16,
    kSetStateInvalidArguments = 17,
    kSetStateCancelled = 18,
    kSetStateFailed = 19,
    kSetStateSuccess = 20,
    kSetStateAlreadyInState = 21,
    kSetStateTransitionToSameState = 22,

    // Responses resulting from unexpected termination
    kFailedUnexpectedTerminationOnEnter = 23,
    kFailedUnexpectedTermination = 24,

    // getInitialMachineState functionality
    kGetInitialMachineStateRequest      = 32,
    kInitialMachineStateNotSet          = 33,
    kInitialMachineStateFailed          = 34,
    kInitialMachineStateSuccess         = 35,

    // getExecutionError functionality
    kGetExecutionErrorRequest = 48,
    kExecutionErrorInvalidArguments = 49,
    kExecutionErrorRequestFailed = 50,
    kExecutionErrorRequestSuccess = 51,

    // validateProcessGroupState
    kValidateProcessGroupState = 64,
    kValidateProcessGroupStateFailed = 65,
    kValidateProcessGroupStateSuccess = 66
};

}  // namespace internal

}  // namespace lcm

}  // namespace score

#endif  // CONTROL_CLIENT_CODES_HPP_INCLUDED
