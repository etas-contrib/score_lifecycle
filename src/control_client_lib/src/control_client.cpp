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

#include "score/concurrency/future/interruptible_future.h"
#include "score/concurrency/future/interruptible_promise.h"
#include <score/lcm/exec_error_domain.h>
#include <score/lcm/control_client.h>

#include <score/lcm/identifier_hash.hpp>
#include "score/mw/com/types.h"
#include "control_client_impl.hpp"

#include <cstdlib>

namespace score {

namespace lcm {

// coverity[exn_spec_violation:FALSE] SetError cannot raise an exception in this instance
inline score::concurrency::InterruptibleFuture<void> GetErrorFuture(score::lcm::ExecErrc errType) noexcept {
    score::concurrency::InterruptiblePromise<void> tmp_{};
    tmp_.SetError(errType);
    return tmp_.GetInterruptibleFuture().value();
}

ControlClient::ControlClient() noexcept {
    try {
        const char* specifier_env = std::getenv("SCORE_LCM_INSTANCE_SPECIFIER");
        std::string specifier_str = specifier_env ? specifier_env : "StateManager/LaunchManager/Instance";
        auto specifier_result = score::mw::com::InstanceSpecifier::Create(std::move(specifier_str));
        if (!specifier_result.has_value())
        {
            control_client_impl_ = nullptr;
            return;
        }
        control_client_impl_ = std::make_unique<ControlClientImpl>(std::move(specifier_result.value()));
    } catch (...) {
        control_client_impl_ = nullptr;
    }
}

ControlClient::~ControlClient() noexcept {
    control_client_impl_.reset();
}

ControlClient::ControlClient(ControlClient&& rval) noexcept {
    control_client_impl_ = std::move(rval.control_client_impl_);
    rval.control_client_impl_ = nullptr;
}

ControlClient& ControlClient::operator=(ControlClient&& rval) noexcept = default;


score::concurrency::InterruptibleFuture<void> ControlClient::ActivateRunTarget(std::string_view runTargetName) const noexcept
{
    score::concurrency::InterruptibleFuture<void> retVal_ {};

    if( control_client_impl_ != nullptr )
    {
        static IdentifierHash pg_name{"MainPG"};
        IdentifierHash pg_state{"MainPG/" + std::string(runTargetName)};
        retVal_ = control_client_impl_->SetState(pg_name, pg_state);
    }
    else
    {
        retVal_ = GetErrorFuture(ExecErrc::kCommunicationError);
    }

    return retVal_;
}

}  // namespace lcm

}  // namespace score
