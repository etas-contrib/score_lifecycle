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
load("@rules_pkg//pkg:mappings.bzl", "pkg_attributes", "pkg_files")
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")
load("@score_itf//:defs.bzl", "py_itf_test")
load("@score_lifecycle_pip//:requirements.bzl", "all_requirements")
load("@score_tooling//:defs.bzl", "score_py_pytest")
load("//:defs.bzl", "launch_manager_config")
load("//config:flatbuffers_rules.bzl", "flatbuffer_json_to_bin")
load("//tests/utils/bazel:integration.bzl", "integration_test")

launch_manager_config(
    name = "lm_no_comms_config",
    config = ":no_comms_config.json",
    flatbuffer_out_dir = "etc",
)

cc_binary(
    name = "no_comms_process",
    srcs = ["no_comms_process.cpp"],
    deps = [
        "//src/launch_manager_daemon/common:osal",
        "//src/launch_manager_daemon/lifecycle_client_lib:lifecycle_client",
        "//tests/utils/test_helper",
        "@googletest//:gtest_main",
    ],
)

pkg_files(
    name = "no_comms_main_files",
    srcs = [
        ":no_comms_process",
        "//src/launch_manager_daemon:launch_manager",
    ],
    attributes = pkg_attributes(mode = "0755"),
    prefix = "tests/incorrect_config_no_comms",
)

pkg_files(
    name = "no_comms_etc_files",
    srcs = [":lm_no_comms_config"],
    prefix = "tests/incorrect_config_no_comms",
)

pkg_tar(
    name = "no_comms_binaries",
    srcs = [
        ":no_comms_etc_files",
        ":no_comms_main_files",
    ],
)

integration_test(
    name = "incorrect_config_no_comms",
    srcs = ["test_incorrect_config_no_comms.py"],
    tags = ["integration"],
    test_binaries = ":no_comms_binaries",
    deps = ["//tests/utils/testing_utils"],
)
