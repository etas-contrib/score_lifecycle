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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sched.h>
#include <secpol/secpol.h>
#include <spawn.h>
#include <sys/resource.h>
#include <unistd.h>
#include <signal.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "score/mw/launch_manager/osal/start_process.hpp"

using namespace testing;

using namespace score::mw::lifecycle::internal::osal;
using namespace score::mw::lifecycle::internal;

// NOLINTBEGIN

class SpawnMock
{
  public:
    MOCK_METHOD(
        int,
        posix_spawn,
        (pid_t*,
         const char*,
         const posix_spawn_file_actions_t*,
         const posix_spawnattr_t*,
         char* const[],
         char* const[]),
        ());
    MOCK_METHOD(int, posix_spawn_file_actions_adddup2, (posix_spawn_file_actions_t*, int, int), ());
    MOCK_METHOD(int, secpol_posix_spawnattr_settypeid, (secpol_file_t*, posix_spawnattr_t*, const char*, uint32_t), ());
    MOCK_METHOD(int, setgroupspid, (int, const gid_t*, pid_t), ());
    MOCK_METHOD(int, prlimit, (pid_t, int, const struct rlimit*, struct rlimit*), ());
    MOCK_METHOD(int, kill, (pid_t, int), ());
};

std::unique_ptr<SpawnMock> g_spawn_mock = nullptr;

extern "C" {
int __real_posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t*, int, int);

int __wrap_posix_spawn(
    pid_t* pid,
    const char* path,
    const posix_spawn_file_actions_t* file_actions,
    const posix_spawnattr_t* attrp,
    char* const argv[],
    char* const envp[])
{
    return g_spawn_mock->posix_spawn(pid, path, file_actions, attrp, argv, envp);
}

int __wrap_posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t* file_actions, int fd, int new_fd)
{
    const int rc = g_spawn_mock->posix_spawn_file_actions_adddup2(file_actions, fd, new_fd);
    return rc == 0 ? __real_posix_spawn_file_actions_adddup2(file_actions, fd, new_fd) : rc;
}

int __wrap_secpol_posix_spawnattr_settypeid(
    secpol_file_t* handle,
    posix_spawnattr_t* attrp,
    const char* name,
    uint32_t flags)
{
    return g_spawn_mock->secpol_posix_spawnattr_settypeid(handle, attrp, name, flags);
}

int __wrap_setgroupspid(int gidsetsize, const gid_t* grouplist, pid_t pid)
{
    return g_spawn_mock->setgroupspid(gidsetsize, grouplist, pid);
}

int __wrap_prlimit(pid_t pid, int resource, const struct rlimit* new_limit, struct rlimit* old_limit)
{
    return g_spawn_mock->prlimit(pid, resource, new_limit, old_limit);
}

int __wrap_kill(pid_t pid, int sig)
{
    return g_spawn_mock->kill(pid, sig);
}
}

// NOLINTEND

namespace
{

/// @brief The spawn attributes, read back with the real getters while the attribute object is valid.
struct CapturedSpawn
{
    std::uint32_t xflags{};
    pid_t pgroup{-1};
    int sched_policy{-1};
    int sched_priority{-1};
    std::uint64_t runmask{};
    uid_t uid{};
    gid_t gid{};
    std::uint32_t stackmax{};
    std::string path;
    std::vector<std::string> argv;
};

}  // namespace

class QnxStartProcessTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        RecordProperty("TestType", "interface-test");
        RecordProperty("DerivationTechnique", "equivalence-classes");

        g_spawn_mock = std::make_unique<StrictMock<SpawnMock>>();

        config_.name = "TestComponent";
        config_.component_properties.binary_name = "TestProcess";
        config_.deployment_config.process_command = configuration::ProcessCommand{"/bin/TestProcess", {"-a", "b"}};
        config_.deployment_config.working_dir = "/";
        config_.deployment_config.sandbox.scheduling_policy = SCHED_RR;
        config_.deployment_config.sandbox.scheduling_priority = 10;
        config_.deployment_config.sandbox.uid = 11;
        config_.deployment_config.sandbox.gid = 13;
    }

    void TearDown() override
    {
        g_spawn_mock.reset();
    }

    /// @brief Expects a successful posix_spawn that captures the spawn attributes.
    void ExpectSpawnSucceeds()
    {
        EXPECT_CALL(*g_spawn_mock, posix_spawn)
            .WillOnce([this](
                          pid_t* pid,
                          const char* path,
                          const posix_spawn_file_actions_t*,
                          const posix_spawnattr_t* attrp,
                          char* const argv[],
                          char* const[]) {
                EXPECT_EQ(posix_spawnattr_getxflags(attrp, &captured_.xflags), 0);
                EXPECT_EQ(posix_spawnattr_getpgroup(attrp, &captured_.pgroup), 0);
                EXPECT_EQ(posix_spawnattr_getschedpolicy(attrp, &captured_.sched_policy), 0);
                sched_param param{};
                EXPECT_EQ(posix_spawnattr_getschedparam(attrp, &param), 0);
                captured_.sched_priority = param.sched_priority;
                EXPECT_EQ(pthread_spawnattr_getrunmask_np(attrp, &captured_.runmask), 0);
                EXPECT_EQ(posix_spawnattr_getcred(attrp, &captured_.uid, &captured_.gid), 0);
                EXPECT_EQ(posix_spawnattr_getstackmax(attrp, &captured_.stackmax), 0);
                captured_.path = path;
                for (std::size_t i = 0U; argv[i] != nullptr; ++i)
                {
                    captured_.argv.emplace_back(argv[i]);
                }
                *pid = kChildPid;
                return 0;
            });
    }

    void ExpectHeldProcessConfigured()
    {
        EXPECT_CALL(*g_spawn_mock, setgroupspid(_, _, kChildPid)).WillOnce(Return(0));
        EXPECT_CALL(*g_spawn_mock, prlimit(kChildPid, _, _, nullptr)).Times(AnyNumber()).WillRepeatedly(Return(0));
    }

    IpcCommsSync* StubComms()
    {
        std::memset(test_buffer_.data(), 0, test_buffer_.size());
        auto* block = reinterpret_cast<IpcCommsSync*>(test_buffer_.data());
        block->comms_type_ = CommsType::kReporting;
        comms_ = IpcCommsP{block, [](IpcCommsSync*) {
                           }};
        comms_fd_ = kShmFd;
        return block;
    }

    OsalReturnType StartProcess()
    {
        return startProcess(pid_, config_, comms_fd_, comms_);
    }

    static constexpr pid_t kChildPid = 4711;
    static constexpr int kShmFd = 123;

    configuration::ComponentConfig config_ = {};
    ProcessID pid_{};
    int comms_fd_{-1};
    IpcCommsP comms_;
    CapturedSpawn captured_;

    alignas(IpcCommsSync) std::array<std::byte, sizeof(IpcCommsSync)> test_buffer_;
};

TEST_F(QnxStartProcessTest, spawnsHeldProcessWithConfiguredAttributes)
{
    RecordProperty(
        "Description",
        "Verify that the process is spawned held, with the configured process group, scheduling, "
        "credentials, working directory and arguments, and then continued");

    ExpectSpawnSucceeds();
    ExpectHeldProcessConfigured();
    EXPECT_CALL(*g_spawn_mock, kill(kChildPid, SIGCONT)).WillOnce(Return(0));

    EXPECT_EQ(StartProcess(), OsalReturnType::kSuccess);
    EXPECT_EQ(pid_, kChildPid);

    const std::uint32_t expected_flags = POSIX_SPAWN_HOLD | POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSCHEDULER |
                                         POSIX_SPAWN_SETSCHEDPARAM | POSIX_SPAWN_SETCRED | POSIX_SPAWN_SETCWD;
    EXPECT_EQ(captured_.xflags & expected_flags, expected_flags);
    EXPECT_EQ(captured_.xflags & static_cast<std::uint32_t>(POSIX_SPAWN_EXPLICIT_CPU), 0U);
    EXPECT_EQ(captured_.xflags & static_cast<std::uint32_t>(POSIX_SPAWN_SETSTACKMAX), 0U);
    EXPECT_EQ(captured_.pgroup, 0);
    EXPECT_EQ(captured_.sched_policy, SCHED_RR);
    EXPECT_EQ(captured_.sched_priority, 10);
    EXPECT_EQ(captured_.uid, 11U);
    EXPECT_EQ(captured_.gid, 13U);
    EXPECT_EQ(captured_.path, "/bin/TestProcess");
    EXPECT_THAT(captured_.argv, ElementsAre("/bin/TestProcess", "-a", "b"));
}

TEST_F(QnxStartProcessTest, affinityIsAppliedWhenConfigured)
{
    RecordProperty("Description", "Verify that the runmask is set if an affinity mask is configured");

    config_.deployment_config.sandbox.affinity_mask = 0x5U;

    ExpectSpawnSucceeds();
    ExpectHeldProcessConfigured();
    EXPECT_CALL(*g_spawn_mock, kill(kChildPid, SIGCONT)).WillOnce(Return(0));

    EXPECT_EQ(StartProcess(), OsalReturnType::kSuccess);
    EXPECT_NE(captured_.xflags & static_cast<std::uint32_t>(POSIX_SPAWN_EXPLICIT_CPU), 0U);
    EXPECT_EQ(captured_.runmask, 0x5U);
}

TEST_F(QnxStartProcessTest, commsFdIsDuplicatedAndPidStoredBeforeContinue)
{
    RecordProperty(
        "Description",
        "Verify that the comms fd is duplicated to sync_fd and that the pid is stored in the shared memory "
        "block before the held process is continued");

    IpcCommsSync* block = StubComms();

    EXPECT_CALL(*g_spawn_mock, posix_spawn_file_actions_adddup2(_, kShmFd, IpcCommsSync::sync_fd)).WillOnce(Return(0));
    ExpectSpawnSucceeds();
    ExpectHeldProcessConfigured();
    EXPECT_CALL(*g_spawn_mock, kill(kChildPid, SIGCONT)).WillOnce([block](pid_t, int) {
        EXPECT_EQ(block->pid_, kChildPid);
        return 0;
    });

    EXPECT_EQ(StartProcess(), OsalReturnType::kSuccess);
    EXPECT_EQ(block->pid_, kChildPid);
}

TEST_F(QnxStartProcessTest, noCommsDoesNotDuplicateFd)
{
    RecordProperty("Description", "Verify that no fd is duplicated for a process without comms");

    EXPECT_CALL(*g_spawn_mock, posix_spawn_file_actions_adddup2).Times(0);
    ExpectSpawnSucceeds();
    ExpectHeldProcessConfigured();
    EXPECT_CALL(*g_spawn_mock, kill(kChildPid, SIGCONT)).WillOnce(Return(0));

    EXPECT_EQ(StartProcess(), OsalReturnType::kSuccess);
}

TEST_F(QnxStartProcessTest, supplementaryGroupsAreCleared)
{
    RecordProperty(
        "Description", "Verify that the supplementary groups are explicitly cleared when none are configured");

    ExpectSpawnSucceeds();
    EXPECT_CALL(*g_spawn_mock, setgroupspid(0, nullptr, kChildPid)).WillOnce(Return(0));
    EXPECT_CALL(*g_spawn_mock, kill(kChildPid, SIGCONT)).WillOnce(Return(0));

    EXPECT_EQ(StartProcess(), OsalReturnType::kSuccess);
}

TEST_F(QnxStartProcessTest, groupsAndLimitsAreAppliedBeforeContinue)
{
    RecordProperty(
        "Description",
        "Verify that the supplementary groups and resource limits are applied to the held process before "
        "it is continued, and that a stack limit is also applied via the spawn attributes");

    const std::vector<gid_t> sgids = {1, 2, 3};
    config_.deployment_config.sandbox.supplementary_group_ids = sgids;
    config_.deployment_config.sandbox.rlimits = {
        {RLIMIT_NOFILE, {64U, 128U}},
        {RLIMIT_STACK, {65536U, RLIM_INFINITY}},
    };

    ExpectSpawnSucceeds();
    {
        InSequence seq;
        EXPECT_CALL(*g_spawn_mock, setgroupspid(3, NotNull(), kChildPid))
            .WillOnce([&sgids](int n, const gid_t* groups, pid_t) {
                EXPECT_TRUE(std::equal(groups, groups + n, sgids.begin()));
                return 0;
            });
        EXPECT_CALL(
            *g_spawn_mock,
            prlimit(
                kChildPid,
                RLIMIT_NOFILE,
                AllOf(Field(&rlimit::rlim_cur, 64U), Field(&rlimit::rlim_max, 128U)),
                nullptr))
            .WillOnce(Return(0));
        EXPECT_CALL(*g_spawn_mock, prlimit(kChildPid, RLIMIT_STACK, _, nullptr)).WillOnce(Return(0));
        EXPECT_CALL(*g_spawn_mock, kill(kChildPid, SIGCONT)).WillOnce(Return(0));
    }

    EXPECT_EQ(StartProcess(), OsalReturnType::kSuccess);
    EXPECT_NE(captured_.xflags & static_cast<std::uint32_t>(POSIX_SPAWN_SETSTACKMAX), 0U);
    EXPECT_EQ(captured_.stackmax, 65536U);
}

TEST_F(QnxStartProcessTest, prlimitFailureKillsHeldProcess)
{
    RecordProperty(
        "Description", "Verify that the held process is killed but reported as started if applying a limit fails");

    config_.deployment_config.sandbox.rlimits = {
        {RLIMIT_CPU, {500U, 500U}},
    };

    ExpectSpawnSucceeds();
    EXPECT_CALL(*g_spawn_mock, setgroupspid).WillOnce(Return(0));
    EXPECT_CALL(*g_spawn_mock, prlimit).WillOnce(SetErrnoAndReturn(EPERM, -1));
    EXPECT_CALL(*g_spawn_mock, kill(kChildPid, SIGKILL)).WillOnce(Return(0));
    EXPECT_CALL(*g_spawn_mock, kill(kChildPid, SIGCONT)).Times(0);

    EXPECT_EQ(StartProcess(), OsalReturnType::kSuccess);
    EXPECT_EQ(pid_, kChildPid);
}

TEST_F(QnxStartProcessTest, setgroupspidFailureKillsHeldProcess)
{
    RecordProperty(
        "Description",
        "Verify that the held process is killed but reported as started if setting the supplementary groups fails");

    ExpectSpawnSucceeds();
    EXPECT_CALL(*g_spawn_mock, setgroupspid).WillOnce(SetErrnoAndReturn(EPERM, -1));
    EXPECT_CALL(*g_spawn_mock, kill(kChildPid, SIGKILL)).WillOnce(Return(0));
    EXPECT_CALL(*g_spawn_mock, kill(kChildPid, SIGCONT)).Times(0);

    EXPECT_EQ(StartProcess(), OsalReturnType::kSuccess);
    EXPECT_EQ(pid_, kChildPid);
}

TEST_F(QnxStartProcessTest, spawnFailureReturnsFail)
{
    RecordProperty("Description", "Verify that startProcess fails if posix_spawn fails");

    EXPECT_CALL(*g_spawn_mock, posix_spawn).WillOnce(Return(ENOENT));
    EXPECT_CALL(*g_spawn_mock, setgroupspid).Times(0);
    EXPECT_CALL(*g_spawn_mock, kill).Times(0);

    EXPECT_EQ(StartProcess(), OsalReturnType::kFail);
}

TEST_F(QnxStartProcessTest, invalidWorkingDirReturnsFail)
{
    RecordProperty("Description", "Verify that startProcess fails if the working directory cannot be opened");

    config_.deployment_config.working_dir = "/this/directory/does/not/exist";

    EXPECT_CALL(*g_spawn_mock, posix_spawn).Times(0);

    EXPECT_EQ(StartProcess(), OsalReturnType::kFail);
}

TEST_F(QnxStartProcessTest, securityPolicyIsApplied)
{
    RecordProperty("Description", "Verify that the configured security policy is set as type in the spawn attributes");

    config_.deployment_config.sandbox.security_policy = "app_t";

    EXPECT_CALL(*g_spawn_mock, secpol_posix_spawnattr_settypeid(nullptr, NotNull(), StrEq("app_t"), SECPOL_TYPE_NAME))
        .WillOnce(Return(0));
    ExpectSpawnSucceeds();
    ExpectHeldProcessConfigured();
    EXPECT_CALL(*g_spawn_mock, kill(kChildPid, SIGCONT)).WillOnce(Return(0));

    EXPECT_EQ(StartProcess(), OsalReturnType::kSuccess);
}

TEST_F(QnxStartProcessTest, securityPolicyNotSupportedIsIgnored)
{
    RecordProperty("Description", "Verify that a missing security policy support (ENOTSUP) is ignored");

    config_.deployment_config.sandbox.security_policy = "app_t";

    EXPECT_CALL(*g_spawn_mock, secpol_posix_spawnattr_settypeid).WillOnce(SetErrnoAndReturn(ENOTSUP, -1));
    ExpectSpawnSucceeds();
    ExpectHeldProcessConfigured();
    EXPECT_CALL(*g_spawn_mock, kill(kChildPid, SIGCONT)).WillOnce(Return(0));

    EXPECT_EQ(StartProcess(), OsalReturnType::kSuccess);
}

TEST_F(QnxStartProcessTest, securityPolicyFailureReturnsFail)
{
    RecordProperty("Description", "Verify that startProcess fails if the security policy type cannot be set");

    config_.deployment_config.sandbox.security_policy = "unknown_t";

    EXPECT_CALL(*g_spawn_mock, secpol_posix_spawnattr_settypeid).WillOnce(SetErrnoAndReturn(EINVAL, -1));
    EXPECT_CALL(*g_spawn_mock, posix_spawn).Times(0);

    EXPECT_EQ(StartProcess(), OsalReturnType::kFail);
}
