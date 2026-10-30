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

#ifndef OSAL_START_PROCESS_HPP_INCLUDED
#define OSAL_START_PROCESS_HPP_INCLUDED

#include "score/mw/launch_manager/osal/iprocess_starter.hpp"

#include <memory>

namespace score::mw::lifecycle::internal::osal
{

/// @brief Creates the IProcessStarter of the current platform using the production OS implementations.
/// @details Linux uses fork() and execve(); QNX uses posix_spawn().
/// @return The process starter, never null.
std::unique_ptr<IProcessStarter> createProcessStarter();

}  // namespace score::mw::lifecycle::internal::osal

#endif  // OSAL_START_PROCESS_HPP_INCLUDED
