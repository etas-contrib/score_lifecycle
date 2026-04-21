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

#ifndef CONTROL_CLIENT_IMPL_H_
#define CONTROL_CLIENT_IMPL_H_

#include <mutex>
#include <optional>
#include <cstdint>

#include <score/lcm/control_client.h>
#include <score/lcm/control_service.h>
#include <score/lcm/identifier_hash.hpp>

#include "score/concurrency/future/interruptible_future.h"
#include "score/concurrency/future/interruptible_promise.h"
#include "score/mw/com/types.h"

namespace score {

namespace lcm {

class ControlClientImpl final {
   public:
    ControlClientImpl() = delete;

    explicit ControlClientImpl(const score::mw::com::InstanceSpecifier& instance_specifier) noexcept;

    ~ControlClientImpl() noexcept;

    ControlClientImpl(const ControlClientImpl&) = delete;
    ControlClientImpl& operator=(const ControlClientImpl&) = delete;
    ControlClientImpl(ControlClientImpl&& rval) = delete;
    ControlClientImpl& operator=(ControlClientImpl&& rval) = delete;

    score::concurrency::InterruptibleFuture<void> SetState(const IdentifierHash& pg_name, const IdentifierHash& pg_state) noexcept;

   private:
    void OnActivationResult();

    std::mutex mutex_;
    std::optional<ControlProxy> proxy_;
    std::optional<score::concurrency::InterruptiblePromise<void>> pending_promise_;
    uint32_t expected_request_id_{0};
    uint32_t next_request_id_{1};
};

}  // namespace lcm

}  // namespace score

#endif  // CONTROL_CLIENT_IMPL_H_
