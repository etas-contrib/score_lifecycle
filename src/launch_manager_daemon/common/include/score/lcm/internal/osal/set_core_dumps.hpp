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


#ifndef SET_CORE_DUMPS_HPP_INCLUDED
#define SET_CORE_DUMPS_HPP_INCLUDED

#include <cstdint>

namespace score {

namespace lcm {

namespace internal {

namespace osal {

/// @brief Re-enable the dumpable flag after setuid(), which may clear it.
/// @details This doesn't need to be done on QNX and so this is a NO-OP.
/// @returns 0 on success, -1 on failure.
std::int32_t setCoreDumps() noexcept(true);

}  // namespace osal
}  // namespace internal
}  // namespace lcm
}  // namespace score

#endif
