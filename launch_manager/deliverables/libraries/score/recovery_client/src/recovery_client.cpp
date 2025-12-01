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


#include <score/lcm/recovery_client.h>
#include <score/lcm/internal/recovery_client_impl.hpp>

namespace score {
namespace lcm {
RecoveryClient::RecoveryClient() noexcept{
    try {
        recovery_client_impl_ = std::make_unique<internal::RecoveryClientImpl>();
    } catch (...) {
        recovery_client_impl_ = nullptr;
    }
}

RecoveryClient::~RecoveryClient() noexcept {
    recovery_client_impl_.reset();
}

score::concurrency::InterruptibleFuture<void> RecoveryClient::sendRecoveryRequest(
            const score::lcm::IdentifierHash& pg_name, const score::lcm::IdentifierHash& pg_state) noexcept {
    return recovery_client_impl_->sendRecoveryRequest(pg_name, pg_state);
}

void RecoveryClient::setResponseSuccess(std::size_t promise_id) noexcept {
    recovery_client_impl_->setResponseSuccess(promise_id);
}

void RecoveryClient::setResponseError(std::size_t promise_id, score::lcm::ExecErrc errType) noexcept {
    recovery_client_impl_->setResponseError(promise_id, errType);
}

RecoveryRequest* RecoveryClient::getNextRequest() noexcept {
    return recovery_client_impl_->getNextRequest();
}
}
}