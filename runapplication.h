// *******************************************************************************
// Copyright (c) 2025 Contributors to the Eclipse Foundation
//
// See the NOTICE file(s) distributed with this work for additional
// information regarding copyright ownership.
//
// This program and the accompanying materials are made available under the
// terms of the Apache License Version 2.0 which is available at
// https://www.apache.org/licenses/LICENSE-2.0
//
// SPDX-License-Identifier: Apache-2.0
// *******************************************************************************

#ifndef PLATFORM_AAS_MW_LIFECYCLE_RUNAPPLICATION_H
#define PLATFORM_AAS_MW_LIFECYCLE_RUNAPPLICATION_H

#include "platform/aas/lib/memory/string_literal.h"
#include "platform/aas/mw/lifecycle/applicationcontext.h"

#include <cstdint>

namespace bmw
{
namespace mw
{
namespace lifecycle
{

template <typename ApplicationType>
class Run final
{
  public:
    Run(const std::int32_t argc,
        const bmw::StringLiteral*
            argv) /* NOLINT(modernize-avoid-c-arrays): array tolerated for command line arguments */
        : context_{argc, argv}
    {
    }

    template <typename... Args>
    std::int32_t AsPosixProcess(Args&&... args) const
    {
        bmw::mw::lifecycle::LifeCycleManager lifecycle_manager{};
        /* Branching in below line is due to hidden exception handling */
        return InstantiateAndRunApplication(lifecycle_manager, std::forward<Args>(args)...);  // LCOV_EXCL_BR_LINE
    }

  private:
    template <typename... Args>
    std::int32_t InstantiateAndRunApplication(bmw::mw::lifecycle::LifeCycleManager& lifecycle_manager,
                                              Args&&... args) const
    {
        /* KW_SUPPRESS_START:MISRA.VAR.NEEDS.CONST:False positive:app is passed as const reference to run(). */
        ApplicationType app{std::forward<Args>(args)...};
        /* KW_SUPPRESS_END:MISRA.VAR.NEEDS.CONST */
        return lifecycle_manager.run(app, context_);  // LCOV_EXCL_BR_LINE
    }

    const bmw::mw::lifecycle::ApplicationContext context_;
};

/**
 * \brief Abstracts initialization and running of an application with LifeCycleManager.
 */
template <typename ApplicationType, typename... Args>

/* NOLINTNEXTLINE(modernize-avoid-c-arrays): array tolerated for command line arguments */
std::int32_t run_application(const std::int32_t argc, const bmw::StringLiteral argv[], Args&&... args)
{
    return 0;
}

}  // namespace lifecycle
}  // namespace mw
}  // namespace bmw

#endif  // PLATFORM_AAS_MW_LIFECYCLE_RUNAPPLICATION_H
