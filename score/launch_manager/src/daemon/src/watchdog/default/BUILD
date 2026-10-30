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
load("//tests/utils/bazel:unit_test.bzl", "lm_cc_test")

# Default watchdog implementation, the default value of //config:watchdog_impl.
# Does not service any hardware watchdog, but publishes a heartbeat counter in shared memory (see Heartbeat in default_watchdog.hpp).
# When fired, it aborts the Launch Manager or reboots the machine, selected with
# --@score_lifecycle//config:default_watchdog_reaction=abort|reboot (default: abort).
cc_library(
    name = "default_watchdog",
    srcs = ["default_watchdog.cpp"],
    hdrs = ["default_watchdog.hpp"],
    include_prefix = "score/mw/launch_manager/watchdog/default",
    local_defines = select({
        "//config:default_watchdog_reaction_reboot": ["LM_DEFAULT_WATCHDOG_REACTION_REBOOT"],
        "//conditions:default": [],
    }),
    strip_include_prefix = "/score/launch_manager/src/daemon/src/watchdog/default",
    visibility = ["//visibility:public"],
    deps = [
        "//score/launch_manager/src/daemon/src/common:log",
        "//score/launch_manager/src/daemon/src/watchdog:i_watchdog",
        "@score_communication//score/memory/shared",
    ],
)

lm_cc_test(
    name = "default_watchdog_UT",
    srcs = ["default_watchdog_UT.cpp"],
    target_compatible_with = select({
        # The test fires the watchdog, which would reboot the test host in the reboot variant.
        "//config:default_watchdog_reaction_reboot": ["@platforms//:incompatible"],
        "//conditions:default": [],
    }),
    deps = [
        ":default_watchdog",
        "@googletest//:gtest_main",
        "@score_communication//score/memory/shared",
    ],
)
