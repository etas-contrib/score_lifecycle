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

#include "platform/aas/mw/lifecycle/applicationcontext.h"
#include <algorithm>
#include <iterator>

bmw::mw::lifecycle::ApplicationContext::ApplicationContext(
    const std::int32_t argc,
    const bmw::StringLiteral argv[])  // NOLINT(modernize-avoid-c-arrays): array tolerated for command line arguments
    : m_args(argv, argv + argc), m_app_path(argv[0])  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic): array
                                                      // tolerated for command line arguments
{
}

const std::vector<std::string>& bmw::mw::lifecycle::ApplicationContext::get_arguments() const noexcept
{
    return m_args;
}

std::string bmw::mw::lifecycle::ApplicationContext::get_argument(const amp::string_view flag) const noexcept
{
    std::string cont_result{""};
    if (!m_args.empty())
    {
        const auto result = std::find_if(m_args.begin(), m_args.end(), [flag](const auto& arg) -> bool {
            return amp::string_view{arg} == flag;
        });
        if ((result != m_args.end()) && (std::next(result) != m_args.end()))
        {
            cont_result = *std::next(result);
        }
    }

    return cont_result;
}
