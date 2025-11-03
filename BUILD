# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************

load("@safe_posix_platform//platform/aas/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "applicationcontext",
    srcs = [
        "applicationcontext.cpp",
    ],
    hdrs = [
        "applicationcontext.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FUSA"],
    deps = [
        "//platform/aas/lib/memory:string_literal",
        "@amp",
    ],
)

cc_library(
    name = "application",
    srcs = [
        "application.cpp",
    ],
    hdrs = [
        "application.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FUSA"],
    deps = [
        ":applicationcontext",
        "@amp",
    ],
)

cc_library(
    name = "aasapplicationcontainer",
    srcs = [
        "aasapplicationcontainer.cpp",
    ],
    hdrs = [
        "aasapplicationcontainer.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":applicationcontext",
        "@amp",
    ],
)

cc_library(
    name = "lifecyclemanager",
    srcs = [
        "lifecyclemanager.cpp",
    ],
    hdrs = [
        "lifecyclemanager.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FUSA"],
    deps = [
        ":application",
        "//platform/aas/lib/os:stdlib",
        "//platform/aas/lib/os/utils:signal",
        "//platform/aas/mw/log",
        "@amp",
    ],
)

[
    cc_library(
        name = name,
        srcs = [
            "runapplication.cpp",
        ],
        hdrs = [
            "runapplication.h",
        ],
        features = COMPILER_WARNING_FEATURES,
        tags = ["FUSA"],
        deps = [
            ":applicationcontext",
            "@amp",
        ] + dependencies,
    )
    for name, dependencies in [
        (
            "lifecycle",
            [
            ],
        ),
        (
            "lifecycle_non_adaptive",
            [
            ],
        ),
    ]
]

filegroup(
    name = "all_sources",
    srcs = glob([
        "*.cpp",
        "*.h",
    ]),
)
