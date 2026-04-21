/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#include <chrono>
#include <csignal>
#include <iostream>
#include <unistd.h>
#include <gtest/gtest.h>
#include <filesystem>
#include <cstdlib>
#include <score/lcm/lifecycle_client.h>
#include <score/lcm/control_client.h>
#include <score/lcm/identifier_hash.hpp>
#include "score/mw/com/runtime.h"
#include "score/mw/com/runtime_configuration.h"
#include "tests/utils/test_helper/test_helper.hpp"

TEST(Smoke, Daemon) {
    score::lcm::ControlClient client;
    TEST_STEP("Control daemon report kRunning") {
        // report kRunning
        auto result = score::lcm::LifecycleClient{}.ReportExecutionState(score::lcm::ExecutionState::kRunning);
        ASSERT_TRUE(result.has_value()) << "client.ReportExecutionState() failed: " << result.error().Message();
    }
    TEST_STEP("Activate RunTarget Running") {
        score::cpp::stop_token stop_token;
        auto future = client.ActivateRunTarget("Running");
        auto wait_result = future.WaitFor(stop_token, std::chrono::milliseconds{500});
        ASSERT_TRUE(wait_result.has_value()) << "Timed out waiting for RunTarget Running activation";
        auto result = future.Get(stop_token);
        EXPECT_TRUE(result.has_value()) << "Activating target Running failed: " << result.error().Message();
    }
    TEST_STEP("Activate RunTarget Startup") {
        score::cpp::stop_token stop_token;
        auto future = client.ActivateRunTarget("Startup");
        auto wait_result = future.WaitFor(stop_token, std::chrono::milliseconds{500});
        ASSERT_TRUE(wait_result.has_value()) << "Timed out waiting for RunTarget Startup activation";
        auto result = future.Get(stop_token);
        EXPECT_TRUE(result.has_value());
    }

    TEST_STEP("Activate RunTarget Off") {
        client.ActivateRunTarget("Off");
    }
}

int main(int argc, char** argv) {
    std::cout << "Running at " << std::filesystem::current_path() << std::endl;
    const char* config_path = std::getenv("SCORE_MW_COM_CONFIG");
    if (config_path) {
        score::mw::com::runtime::RuntimeConfiguration config{score::filesystem::Path{config_path}};
        score::mw::com::runtime::InitializeRuntime(config);
    } else {
        std::cerr << "SCORE_MW_COM_CONFIG environment variable is not set. Please set it to the path of the mw_com configuration file." << std::endl;
        return EXIT_FAILURE;
    }
    return TestRunner(__FILE__, true).RunTests();
}
