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

#ifndef CONTROL_SERVER_HPP_INCLUDED
#define CONTROL_SERVER_HPP_INCLUDED

#include <cstdint>
#include <string_view>

#include <score/lcm/control_service.h>
#include <score/lcm/internal/control_client_codes.hpp>
#include "score/language/safecpp/scoped_function/scope.h"
#include "score/mw/com/types.h"

namespace score {

namespace lcm {

namespace internal {

class IGraphControl;  // forward declaration

class ControlServer final {
   public:
    ControlServer(std::string_view instance_specifier, IGraphControl& graph_control);

    ~ControlServer();

    ControlServer(const ControlServer&) = delete;
    ControlServer& operator=(const ControlServer&) = delete;
    ControlServer(ControlServer&&) = delete;
    ControlServer& operator=(ControlServer&&) = delete;

    bool Initialize();

    void Shutdown();

   private:
    void OnMethodCall(const score::lcm::RunTargetRequest& request);

    void SendResult(uint32_t request_id, ControlClientCode result_code, uint32_t execution_error_code);

    std::optional<ControlSkeleton> skeleton_;
    score::mw::com::InstanceSpecifier instance_specifier_;
    IGraphControl& graph_control_;
    safecpp::Scope<> callback_scope_;
};

}  // namespace internal

}  // namespace lcm

}  // namespace score

#endif  // CONTROL_SERVER_HPP_INCLUDED
