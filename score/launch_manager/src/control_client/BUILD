# *******************************************************************************
# Copyright (c) 2026 Contributors to the Eclipse Foundation
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
load("@rules_cc//cc:defs.bzl", "cc_library")
load("@score_docs_as_code//:docs.bzl", "docs_bundle")

cc_library(
    name = "control_client",
    srcs = [
        "src/control_client.cpp",
    ],
    hdrs = [
        "src/control_client.h",
    ],
    include_prefix = "score/mw/lifecycle",
    strip_include_prefix = "/score/launch_manager/src/control_client/src",
    visibility = ["//score:__subpackages__"],
    deps = [
        "//score/launch_manager/src/control_client/src/details:control_client_impl",
        "//score/launch_manager/src/daemon/src/common:identifier_hash",
    ],
)

docs_bundle(
    name = "docs",
    code_targets = [
        ":control_client",
    ],
    source_dir = "docs",
    visibility = ["//visibility:public"],
)
