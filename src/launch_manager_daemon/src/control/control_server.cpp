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

#include "control_server.hpp"
#include <process_group_manager/IGraphControl.hpp>
#include <score/lcm/exec_error_domain.h>
#include <score/lcm/identifier_hash.hpp>
#include <score/lcm/internal/log.hpp>

namespace score {

namespace lcm {

namespace internal {

static ControlClientCode execErrcToControlClientCode(score::lcm::ExecErrc errc)
{
    switch (errc)
    {
        case score::lcm::ExecErrc::kInvalidArguments:
            return ControlClientCode::kSetStateInvalidArguments;
        case score::lcm::ExecErrc::kCancelled:
            return ControlClientCode::kSetStateCancelled;
        case score::lcm::ExecErrc::kFailed:
            return ControlClientCode::kSetStateFailed;
        case score::lcm::ExecErrc::kAlreadyInState:
            return ControlClientCode::kSetStateAlreadyInState;
        case score::lcm::ExecErrc::kInTransitionToSameState:
            return ControlClientCode::kSetStateTransitionToSameState;
        case score::lcm::ExecErrc::kFailedUnexpectedTerminationOnEnter:
            return ControlClientCode::kFailedUnexpectedTerminationOnEnter;
        case score::lcm::ExecErrc::kFailedUnexpectedTerminationOnExit:
            return ControlClientCode::kFailedUnexpectedTermination;
        default:
            return ControlClientCode::kSetStateFailed;
    }
}

ControlServer::ControlServer(std::string_view instance_specifier, IGraphControl& graph_control)
    : skeleton_{},
      instance_specifier_{score::mw::com::InstanceSpecifier::Create(std::string{instance_specifier}).value()},
      graph_control_{graph_control},
      callback_scope_{}
{
}

ControlServer::~ControlServer() {
    Shutdown();
}

bool ControlServer::Initialize() {
    auto create_result = ControlSkeleton::Create(instance_specifier_);
    if (!create_result.has_value()) {
        LM_LOG_ERROR() << "ControlServer: Failed to create skeleton";
        return false;
    }

    skeleton_.emplace(std::move(create_result).value());

    skeleton_->activate_run_target.RegisterHandler(
        [this](const score::lcm::RunTargetRequest& request) {
            OnMethodCall(request);
        });

    auto offer_result = skeleton_->OfferService();
    if (!offer_result.has_value()) {
        LM_LOG_ERROR() << "ControlServer: Failed to offer service";
        skeleton_.reset();
        return false;
    }

    LM_LOG_DEBUG() << "ControlServer: Initialized and offering service";
    return true;
}

void ControlServer::Shutdown() {
    if (skeleton_.has_value()) {
        skeleton_->StopOfferService();
        skeleton_.reset();
    }
}

void ControlServer::OnMethodCall(const score::lcm::RunTargetRequest& request) {
    LM_LOG_INFO() << "ControlServer: Received ActivateRunTarget request" << request.request_id
                   << " for pg_state_hash=" << request.pg_state_hash;

    uint32_t request_id = request.request_id;
    IdentifierHash target_state = IdentifierHash::FromRaw(request.pg_state_hash);

    auto future = graph_control_.ActivateRunTarget(target_state);

    future.Then({callback_scope_, [this, request_id](score::Result<void>& result) noexcept {
        if (result.has_value())
        {
            SendResult(request_id, ControlClientCode::kSetStateSuccess, 0U);
        }
        else
        {
            auto errc = static_cast<score::lcm::ExecErrc>(*result.error());
            ControlClientCode code = execErrcToControlClientCode(errc);
            SendResult(request_id, code, 0U);
        }
    }});
}

void ControlServer::SendResult(uint32_t request_id, ControlClientCode result_code, uint32_t execution_error_code) {
    if (!skeleton_.has_value()) {
        LM_LOG_WARN() << "ControlServer: Cannot send result, skeleton not available";
        return;
    }

    auto alloc_result = skeleton_->activation_result.Allocate();
    if (alloc_result.has_value()) {
        auto& sample = alloc_result.value();
        sample.Get()->request_id = request_id;
        sample.Get()->result_code = static_cast<uint32_t>(result_code);
        sample.Get()->execution_error_code = execution_error_code;
        skeleton_->activation_result.Send(std::move(alloc_result).value());
    } else {
        LM_LOG_WARN() << "ControlServer: Failed to allocate event sample";
    }
}

}  // namespace internal

}  // namespace lcm

}  // namespace score
