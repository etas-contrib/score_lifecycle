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
#include <sys/prctl.h>

#include <score/lcm/internal/osal/set_core_dumps.hpp>

namespace score::lcm::internal::osal
{

std::int32_t setCoreDumps() noexcept(true)
{
    return ::prctl(PR_SET_DUMPABLE, 1);
}

}  // namespace score::lcm::internal::osal
