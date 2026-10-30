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

#include "score/mw/launch_manager/watchdog/default/default_watchdog.hpp"
#include "score/mw/launch_manager/watchdog/watchdog_factory.hpp"

#include "score/memory/shared/shared_memory_factory.h"

#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <csignal>
#include <string>

namespace score::mw::lifecycle::internal::watchdog
{
namespace
{

using score::memory::shared::SharedMemoryFactory;

class DefaultWatchdogTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        SharedMemoryFactory::RemoveStaleArtefacts(path_);
    }

    void TearDown() override
    {
        watchdog_.deinit();
        SharedMemoryFactory::Clear();
        SharedMemoryFactory::RemoveStaleArtefacts(path_);
    }

    /// Reads the heartbeat counter through a separate POSIX mapping, as an external observer would.
    /// SharedMemoryFactory::Open() can't be used, since it returns the creator's own resource within this process.
    static std::uint64_t readCounter(const std::string& path)
    {
        const int fd = ::shm_open(path.c_str(), O_RDONLY, 0);
        EXPECT_GE(fd, 0);
        struct stat st{};
        EXPECT_EQ(::fstat(fd, &st), 0);
        void* const base = ::mmap(nullptr, static_cast<std::size_t>(st.st_size), PROT_READ, MAP_SHARED, fd, 0);
        ::close(fd);
        EXPECT_NE(base, MAP_FAILED);
        const auto offset = SharedMemoryFactory::GetControlBlockSize();
        const auto* heartbeat = reinterpret_cast<const Heartbeat*>(static_cast<const char*>(base) + offset);
        const auto value = heartbeat->counter.load();
        ::munmap(base, static_cast<std::size_t>(st.st_size));
        return value;
    }

    static bool exists(const std::string& path)
    {
        const int fd = ::shm_open(path.c_str(), O_RDONLY, 0);
        if (fd >= 0)
        {
            ::close(fd);
        }
        return fd >= 0;
    }

    const std::string path_{"/lm_heartbeat_ut_" + std::to_string(::getpid())};
    DefaultWatchdog watchdog_{path_};
};

TEST_F(DefaultWatchdogTest, CreateWatchdogReturnsInstance)
{
    RecordProperty("Description", "createWatchdog() returns a watchdog instance.");
    EXPECT_NE(createWatchdog(0, nullptr), nullptr);
}

TEST_F(DefaultWatchdogTest, ServiceAndDeinitWithoutInit_AreNoOps)
{
    RecordProperty("Description", "serviceWatchdog() and deinit() are safe without prior init().");
    watchdog_.serviceWatchdog();
    watchdog_.deinit();
    EXPECT_FALSE(exists(path_));
}

TEST_F(DefaultWatchdogTest, Init_CreatesWorldReadableUserWritableHeartbeat)
{
    RecordProperty(
        "Description",
        "init() creates the heartbeat region, only writable by the owner and readable by everybody, "
        "with the counter starting at 0.");
    ASSERT_TRUE(watchdog_.init());

    const int fd = ::shm_open(path_.c_str(), O_RDONLY, 0);
    ASSERT_GE(fd, 0);
    struct stat st{};
    ASSERT_EQ(::fstat(fd, &st), 0);
    ::close(fd);
    EXPECT_EQ(st.st_mode & 0777, 0644);
    EXPECT_EQ(readCounter(path_), 0U);
}

TEST_F(DefaultWatchdogTest, Init_RemovesStaleRegion)
{
    RecordProperty("Description", "init() succeeds even if a previous run left the heartbeat region behind.");
    const int fd = ::shm_open(path_.c_str(), O_RDWR | O_CREAT | O_EXCL, 0644);
    ASSERT_GE(fd, 0);
    ::close(fd);

    EXPECT_TRUE(watchdog_.init());
}

TEST_F(DefaultWatchdogTest, ServiceWatchdog_IncrementsHeartbeat)
{
    RecordProperty("Description", "Each serviceWatchdog() call increments the heartbeat counter by one.");
    ASSERT_TRUE(watchdog_.init());
    watchdog_.serviceWatchdog();
    watchdog_.serviceWatchdog();
    watchdog_.serviceWatchdog();
    EXPECT_EQ(readCounter(path_), 3U);
}

TEST_F(DefaultWatchdogTest, Deinit_RemovesHeartbeat)
{
    RecordProperty("Description", "deinit() removes the heartbeat region; serviceWatchdog() afterwards is a no-op.");
    ASSERT_TRUE(watchdog_.init());
    watchdog_.deinit();
    EXPECT_FALSE(exists(path_));
    watchdog_.serviceWatchdog();
}

// Only the abort reaction is tested: running the reboot variant would reboot the test host if the test has the
// privileges to do so.
TEST_F(DefaultWatchdogTest, FireWatchdog_Aborts)
{
    RecordProperty("Description", "With the abort reaction, fireWatchdog() terminates the process with SIGABRT.");
    EXPECT_EXIT(watchdog_.fireWatchdog(), ::testing::KilledBySignal(SIGABRT), "");
}

}  // namespace
}  // namespace score::mw::lifecycle::internal::watchdog
