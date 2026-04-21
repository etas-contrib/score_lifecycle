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

#include <map>

#include "score/concurrency/future/interruptible_future.h"
#include "score/concurrency/future/interruptible_promise.h"
#include <score/lcm/control_client.h>
#include <score/lcm/control_service.h>
#include <score/lcm/identifier_hash.hpp>
#include <score/lcm/internal/log.hpp>
#include "control_client_impl.hpp"

namespace score {

namespace lcm {

// coverity[exn_spec_violation:FALSE] SetError cannot raise an exception in this instance
inline score::concurrency::InterruptibleFuture<void> GetErrorFuture(score::lcm::ExecErrc errType) noexcept
{
    score::concurrency::InterruptiblePromise<void> tmp_ {};
    tmp_.SetError( errType );
    return tmp_.GetInterruptibleFuture().value();
}

// Maps ActivationResult::result_code to ExecErrc
static const std::map<uint32_t, score::lcm::ExecErrc> kResultCodeMap =
{
    { 0U, score::lcm::ExecErrc::kFailed },              // should not happen, 0 = success
    { 17U, score::lcm::ExecErrc::kInvalidArguments },    // kSetStateInvalidArguments
    { 18U, score::lcm::ExecErrc::kCancelled },           // kSetStateCancelled
    { 19U, score::lcm::ExecErrc::kFailed },              // kSetStateFailed
    { 20U, score::lcm::ExecErrc::kFailed },              // kSetStateSuccess (handled separately)
    { 21U, score::lcm::ExecErrc::kAlreadyInState },      // kSetStateAlreadyInState
    { 22U, score::lcm::ExecErrc::kInTransitionToSameState }, // kSetStateTransitionToSameState
    { 23U, score::lcm::ExecErrc::kFailedUnexpectedTerminationOnEnter }, // kFailedUnexpectedTerminationOnEnter
};

ControlClientImpl::ControlClientImpl(const score::mw::com::InstanceSpecifier& instance_specifier) noexcept
    : mutex_{},
      proxy_{},
      pending_promise_{},
      expected_request_id_{0},
      next_request_id_{1}
{
    auto find_result = ControlProxy::FindService(instance_specifier);
    if (find_result.has_value() && !find_result.value().empty()) {
        auto create_result = ControlProxy::Create(find_result.value().front(), {"ActivateRunTarget"});
        if (create_result.has_value()) {
            proxy_.emplace(std::move(create_result).value());

            proxy_->activation_result.Subscribe(1U);
            proxy_->activation_result.SetReceiveHandler([this]() {
                OnActivationResult();
            });
        } else {
            LM_LOG_ERROR() << "ControlClientImpl: Failed to create proxy";
        }
    } else {
        LM_LOG_ERROR() << "ControlClientImpl: FindService failed or no service found";
    }
}

ControlClientImpl::~ControlClientImpl() noexcept {
    if (proxy_.has_value()) {
        proxy_->activation_result.Unsubscribe();
    }
}

void ControlClientImpl::OnActivationResult() {
    if (!proxy_.has_value()) {
        return;
    }

    auto get_result = proxy_->activation_result.GetNewSamples(
        [this](score::mw::com::SamplePtr<ActivationResult> sample) {
            std::lock_guard<std::mutex> lock(mutex_);
             LM_LOG_DEBUG()  << "Received new event with code" << static_cast<int>(sample->result_code);
            if (!pending_promise_.has_value()) {
                return;
            }

            if (sample->request_id != expected_request_id_) {
                 LM_LOG_WARN()  << "Received response with unexpected request_id: " << static_cast<int>(sample->request_id)
                          << ", expected: " << static_cast<int>(expected_request_id_);
                return;
            }

            // Success case
            if (sample->result_code == 20U) {  // kSetStateSuccess
                pending_promise_->SetValue();
            } else {
                auto it = kResultCodeMap.find(sample->result_code);
                if (it != kResultCodeMap.end()) {
                    pending_promise_->SetError(it->second);
                } else {
                    LM_LOG_WARN() << "ControlClient: Unexpected result_code from LaunchManager:" << static_cast<int>(sample->result_code);
                    pending_promise_->SetError(score::lcm::ExecErrc::kFailed);
                }
            }
            pending_promise_.reset();
        },
        1U);
}

score::concurrency::InterruptibleFuture<void> ControlClientImpl::SetState(const IdentifierHash& pg_name, const IdentifierHash& pg_state) noexcept {
    if (!proxy_.has_value()) {
        return GetErrorFuture(ExecErrc::kCommunicationError);
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Cancel any outstanding request
    if (pending_promise_.has_value()) {
        pending_promise_->SetError(ExecErrc::kCancelled);
        pending_promise_.reset();
    }

    // Create new promise and future
    pending_promise_.emplace();
    auto future_result = pending_promise_->GetInterruptibleFuture();
    if (!future_result.has_value()) {
        pending_promise_.reset();
        return GetErrorFuture(ExecErrc::kFailed);
    }

    expected_request_id_ = next_request_id_;
    next_request_id_++;

    RunTargetRequest request{};
    request.request_id = expected_request_id_;
    request.pg_name_hash = pg_name.data();
    request.pg_state_hash = pg_state.data();

    proxy_->activate_run_target(request);

    return std::move(future_result).value();
}

}  // namespace lcm

}  // namespace score
