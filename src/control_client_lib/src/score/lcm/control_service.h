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

#ifndef SCORE_LCM_CONTROL_SERVICE_H_
#define SCORE_LCM_CONTROL_SERVICE_H_

#include "score/mw/com/types.h"

#include <cstdint>

namespace score::lcm {

struct RunTargetRequest {
    uint32_t request_id;
    uint64_t pg_name_hash;
    uint64_t pg_state_hash;
};

struct ActivationResult {
    uint32_t request_id;
    uint32_t result_code;
    uint32_t execution_error_code;
};

template <typename Trait>
class ControlService : public Trait::Base {
  public:
    using Trait::Base::Base;

    typename Trait::template Method<void(RunTargetRequest)>
        activate_run_target{*this, "ActivateRunTarget"};

    typename Trait::template Event<ActivationResult>
        activation_result{*this, "ActivationResult"};
};

using ControlProxy = score::mw::com::AsProxy<ControlService>;
using ControlSkeleton = score::mw::com::AsSkeleton<ControlService>;

}  // namespace score::lcm

#endif  // SCORE_LCM_CONTROL_SERVICE_H_
