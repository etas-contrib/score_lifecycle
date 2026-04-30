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

#include <fcntl.h>
#include <gtest/gtest.h>
#include <score/lcm/lifecycle_client.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tests/utils/test_helper/test_helper.hpp"
#include <csignal>

TEST(NoComms, Process)
{
    auto fd = shm_open("some_shared_memory", O_CREAT | O_RDWR, 0);
    shm_unlink("some_shared_memory");

    // Invalid kRunning report
    auto result = score::lcm::LifecycleClient{}.ReportExecutionState(score::lcm::ExecutionState::kRunning);
    EXPECT_FALSE(result.has_value()) << "client.ReportExecutionState() failed";

    close(fd);
}

int main()
{
    return TestRunner(__FILE__).RunTests();
}
