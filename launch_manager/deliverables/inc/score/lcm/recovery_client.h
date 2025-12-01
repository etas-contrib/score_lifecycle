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


#ifndef SCORE_LCM_RECOVERYCLIENT_H_
#define SCORE_LCM_RECOVERYCLIENT_H_

#include "score/concurrency/future/interruptible_future.h"
#include "score/concurrency/future/interruptible_promise.h"
#include <score/lcm/exec_error_domain.h>
#include <score/lcm/identifier_hash.hpp>

namespace score {
namespace lcm {
namespace internal {
    struct RecoveryClientImpl;
} // namespace internal

struct RecoveryRequest {
    score::lcm::IdentifierHash pg_name_;
    score::lcm::IdentifierHash pg_state_name_;
    std::size_t promise_id_;
};


class RecoveryClient {
public:
    RecoveryClient() noexcept;
    ~RecoveryClient() noexcept;
    RecoveryClient(const RecoveryClient&) = delete;
    RecoveryClient& operator=(const RecoveryClient&) = delete;
    RecoveryClient(RecoveryClient&&) = delete;
    RecoveryClient& operator=(RecoveryClient&&) = delete;

    score::concurrency::InterruptibleFuture<void> sendRecoveryRequest(
        const score::lcm::IdentifierHash& pg_name, const score::lcm::IdentifierHash& pg_state) noexcept;

    void setResponseSuccess(std::size_t promise_id) noexcept;
    void setResponseError(std::size_t promise_id, score::lcm::ExecErrc errType) noexcept;
    RecoveryRequest* getNextRequest() noexcept;

private:
        std::unique_ptr<internal::RecoveryClientImpl> recovery_client_impl_;
}; 
} // namespace lcm
} // namespace score

#endif