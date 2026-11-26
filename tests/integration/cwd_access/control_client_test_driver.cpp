/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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

#include <gtest/gtest.h>

#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <system_error>

#include "tests/utils/test_helper/test_helper.hpp"
#include "tests/utils/test_helper/test_runner.hpp"
#include <score/mw/lifecycle/ilm_control.hpp>
#include <score/mw/lifecycle/report_running.h>

using namespace score::mw::lifecycle;

namespace
{

constexpr const char* kLockedDir = "/tmp/locked";
constexpr const char* kVisibleDir = "/tmp/locked/visible";
constexpr const char* kSecretFile = "/tmp/locked/visible/secret.txt";

void removeLockedDirectory()
{
    std::error_code error;
    std::filesystem::remove_all(kLockedDir, error);
}

}  // namespace

// Given a configuration with a Run Target "cwd_access" that starts cwd_access_process as uid/gid 1000 with the
// working directory /tmp/locked/visible, and a ready condition of "Terminated".
//
// The state manager (running as root) creates /tmp/locked (mode 0700, root only) containing visible/secret.txt
// (accessible to everyone) and then activates "cwd_access". cwd_access_process exits with 0 only if secret.txt is
// neither accessible by its absolute path nor relative to its working directory, so the activation only succeeds
// if the configured working directory does not grant access to the locked directory tree.
TEST(CwdAccess, ControlClientTestDriver)
{
    std::unique_ptr<ILmControl> client;

    TEST_STEP("Create client")
    {
        auto client_result = ILmControl::Create("StateManager/LaunchManager/Instance");
        ASSERT_TRUE(client_result.has_value()) << client_result.error().Message();
        client = std::move(client_result).value();
    }

    TEST_STEP("Register callback")
    {
        const auto result = client->register_run_target_activation_callback(push_event);
        ASSERT_TRUE(result.has_value());
    }

    TEST_STEP("Wait for client connection")
    {
        ASSERT_TRUE(client->wait_for_connection().has_value());
    }

    TEST_STEP("Report running")
    {
        report_running();
    }

    pop_event([](RunTargetActivationSource source, RunTargetName target) {
        TEST_STEP("Callback for Run Target Startup")
        {
            EXPECT_EQ(source, RunTargetActivationSource::kInitialActivation);
            EXPECT_EQ(target, "Startup");
        }
    });

    TEST_STEP("Create the locked directory tree")
    {
        removeLockedDirectory();

        std::error_code error;
        ASSERT_TRUE(std::filesystem::create_directories(kVisibleDir, error)) << error.message();

        {
            std::ofstream secret{kSecretFile};
            secret << "Pa$$w0rd\n";
            ASSERT_TRUE(secret.good()) << "Cannot write " << kSecretFile;
        }

        // /tmp/locked is only accessible to root, but its children are accessible to everyone
        ASSERT_EQ(::chmod(kLockedDir, 0700), 0) << "chmod(" << kLockedDir << ") failed: " << std::strerror(errno);
        ASSERT_EQ(::chmod(kVisibleDir, 0755), 0) << "chmod(" << kVisibleDir << ") failed: " << std::strerror(errno);
        ASSERT_EQ(::chmod(kSecretFile, 0644), 0) << "chmod(" << kSecretFile << ") failed: " << std::strerror(errno);
    }

    TEST_STEP("Activate Run Target starting the unprivileged process inside the locked directory tree")
    {
        const auto result = client->activate_run_target("cwd_access", true);
        EXPECT_TRUE(result.has_value());
    }

    pop_event([](RunTargetActivationSource source, RunTargetName target) {
        TEST_STEP("Callback for Run Target cwd_access")
        {
            EXPECT_EQ(source, RunTargetActivationSource::kStateManagerRequest);
            EXPECT_EQ(target, "cwd_access");
        }
    });

    removeLockedDirectory();
}

int main()
{
    TestRunner::Init();
    const auto result = TestRunner::RunTests();
    TestRunner::EndTest();
    TestRunner::WaitForTermination();
    return result;
}
