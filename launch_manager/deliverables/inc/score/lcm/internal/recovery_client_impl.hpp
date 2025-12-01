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


#ifndef SCORE_LCM_RECOVERYCLIENTIMPL_H_
#define SCORE_LCM_RECOVERYCLIENTIMPL_H_

#include "score/concurrency/future/interruptible_future.h"
#include "score/concurrency/future/interruptible_promise.h"
#include "ipc_dropin/ringbuffer.hpp"
#include <score/lcm/exec_error_domain.h>
#include <score/lcm/recovery_client.h>

#include <iterator>
#include <cstddef>

namespace score {
namespace lcm {
namespace internal {
    struct RecoveryClientRequestInfo {
    score::concurrency::InterruptiblePromise<void>
        promise_;  ///< promise that should be fulfilled, i.e. set_value(), when answer from LCM is available.
    std::atomic_bool
        in_use_;  ///< information whether this slot in the array is used or not. We are using atomic flag as only the code that reserves the slot will use synchronization primitive. There is no reason to protect release code, thanks to the std::atomic_flag
    // Constructor to initialize all members
    RecoveryClientRequestInfo() : promise_(), in_use_(false) {
    }
};

constexpr std::size_t capacity = 1024;
constexpr std::size_t elementSize = sizeof(RecoveryRequest);

class RecoveryClientImpl {
public:
    RecoveryClientImpl() noexcept;
    RecoveryClientImpl(const RecoveryClientImpl&) = delete;
    RecoveryClientImpl& operator=(const RecoveryClientImpl&) = delete;
    RecoveryClientImpl(RecoveryClientImpl&&) = delete;
    RecoveryClientImpl& operator=(RecoveryClientImpl&&) = delete;

    score::concurrency::InterruptibleFuture<void> sendRecoveryRequest(
            const score::lcm::IdentifierHash& pg_name, const score::lcm::IdentifierHash& pg_state) noexcept;

    void setResponseSuccess(std::size_t promise_id) noexcept;
    void setResponseError(std::size_t promise_id, score::lcm::ExecErrc errType) noexcept;
    RecoveryRequest* getNextRequest() noexcept;
private:
    ipc_dropin::RingBuffer<capacity, elementSize> ringBuffer_;  ///< Ring buffer to store recovery requests
    std::array<RecoveryClientRequestInfo, capacity> requests_;
    RecoveryRequest temp_request_;
}; 
} // namespace lcm
}
}

#endif