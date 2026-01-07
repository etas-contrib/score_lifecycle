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

load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "applicationcontext",
    srcs = [
        "src/applicationcontext.cpp",
    ],
    hdrs = [
        "include/applicationcontext.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FUSA"],
    deps = [
        "@score_baselibs//score/memory:string_literal",
        "@amp",
    ],
)

cc_library(
    name = "application",
    srcs = [
        "src/application.cpp",
    ],
    hdrs = [
        "include/application.h",
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
        "src/aasapplicationcontainer.cpp",
    ],
    hdrs = [
        "include/aasapplicationcontainer.h",
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
        "src/lifecyclemanager.cpp",
    ],
    hdrs = [
        "include/lifecyclemanager.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FUSA"],
    deps = [
        ":application",
        "@score_baselibs/score/os:stdlib",
        "@score_baselibs/score/os/utils:signal",
        "@score_baselibs/score/mw/log",
        "@amp",
    ],
)

[
    cc_library(
        name = name,
        srcs = [
            "src/runapplication.cpp",
        ],
        hdrs = [
            "include/runapplication.h",
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
