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

cc_library(
    name = "control_service_types",
    hdrs = ["src/score/lcm/control_service.h"],
    includes = ["src"],
    visibility = [
        "//src/launch_manager_daemon:__pkg__",
        "//tests:__subpackages__",
    ],
    deps = [
        "@score_communication//score/mw/com",
    ],
)

cc_library(
    name = "control_client_lib",
    srcs = [
        "src/control_client.cpp",
        "src/control_client_impl.cpp",
        "src/control_client_impl.hpp",
    ],
    hdrs = [
        "include/score/lcm/control_client.h",
    ],
    includes = ["include"],
    visibility = ["//visibility:public"],
    deps = [
        ":control_service_types",
        "//src/launch_manager_daemon/common:identifier_hash",
        "//src/launch_manager_daemon/common:lifecycle_error",
        "//src/launch_manager_daemon/common:log",
        "@score_baselibs//score/concurrency/future",
        "@score_baselibs//score/mw/log",
        "@score_communication//score/mw/com",
    ],
)
