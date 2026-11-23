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
load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("//tests/utils/bazel:integration.bzl", "integration_test")

# Starts cwd_access_process via osal::startProcess(), which is only implemented for QNX.
cc_binary(
    name = "dummy_launch_manager",
    srcs = ["dummy_launch_manager.cpp"],
    target_compatible_with = ["@platforms//os:qnx"],
    deps = [
        "//score/launch_manager/src/daemon/src/configuration:component_config",
        "//score/launch_manager/src/daemon/src/configuration:environment_config",
        "//score/launch_manager/src/daemon/src/configuration:process_command",
        "//score/launch_manager/src/daemon/src/osal:ipc_comms",
        "//score/launch_manager/src/daemon/src/osal:return_types",
        "//score/launch_manager/src/daemon/src/osal:start_process",
    ],
)

cc_binary(
    name = "cwd_access_process",
    srcs = ["cwd_access_process.cpp"],
    target_compatible_with = ["@platforms//os:qnx"],
)

integration_test(
    name = "cwd_access",
    timeout = "short",
    srcs = ["cwd_access.py"],
    binaries = [
        ":cwd_access_process",
        ":dummy_launch_manager",
    ],
    target_compatible_with = ["@platforms//os:qnx"],
)
