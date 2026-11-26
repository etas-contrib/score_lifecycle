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

# The scenario is specific to QNX: the launch manager opens the working directory with its own (root) privileges and
# passes it to posix_spawn(). On Linux, the working directory is changed after the privileges have been dropped, so
# the process would not start at all.

cc_binary(
    name = "control_client_test_driver",
    srcs = ["control_client_test_driver.cpp"],
    target_compatible_with = ["@platforms//os:qnx"],
    deps = [
        "//score/launch_manager:control_cc",
        "//score/launch_manager:lifecycle_cc",
        "//tests/utils/test_helper",
        "@googletest//:gtest_main",
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
    srcs = ["test.py"],
    binaries = [
        ":control_client_test_driver",
        ":cwd_access_process",
        "//score/launch_manager",
    ],
    config = ":config.json",
    target_compatible_with = ["@platforms//os:qnx"],
)
