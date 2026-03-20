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
#ifndef SCORE_LCM_IRECOVERYCLIENT_H_
#define SCORE_LCM_IRECOVERYCLIENT_H_

#include <optional>
#include <score/lcm/identifier_hash.hpp>

namespace score {
namespace lcm {

struct RecoveryRequest {
    /// @brief The id of the process group the failed process is running in
    score::lcm::IdentifierHash process_group_identifier_;
};

class IRecoveryClient {
public:
    IRecoveryClient() noexcept = default;
    ~IRecoveryClient() noexcept = default;
    IRecoveryClient(const IRecoveryClient&) = delete;
    IRecoveryClient& operator=(const IRecoveryClient&) = delete;
    IRecoveryClient(IRecoveryClient&&) = delete;
    IRecoveryClient& operator=(IRecoveryClient&&) = delete;

    virtual bool sendRecoveryRequest(const score::lcm::IdentifierHash& process_group_identifier) noexcept = 0;
    virtual std::optional<RecoveryRequest> getNextRequest() noexcept = 0;
    virtual bool hasOverflow() const noexcept = 0;
};
} // namespace lcm
} // namespace score

#endif
