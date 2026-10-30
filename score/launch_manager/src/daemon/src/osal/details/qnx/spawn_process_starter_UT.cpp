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
#include <utility>
#include <vector>

#include "score/mw/launch_manager/osal/details/qnx/spawn_process_starter.hpp"
#include "score/os/fcntl_impl.h"
#include "score/os/mocklib/fcntl_mock.h"
#include "score/os/mocklib/qnx/mock_secpol.h"
#include "score/os/mocklib/qnx/mock_unistd.h"
#include "score/os/mocklib/spawn_mock.h"
#include "score/os/mocklib/unistdmock.h"
#include "score/os/spawn_impl.h"
#include "score/os/unistd.h"

using namespace testing;

using namespace score::mw::lifecycle::internal::osal;
using namespace score::mw::lifecycle::internal;

// NOLINTBEGIN

/// @brief Mock for the functions without a baselibs abstraction, replaced at link time.
class ProcessControlMock
{
  public:
    MOCK_METHOD(int, prlimit, (pid_t, int, const struct rlimit*, struct rlimit*), ());
    MOCK_METHOD(int, kill, (pid_t, int), ());
};

std::unique_ptr<ProcessControlMock> g_process_control_mock = nullptr;

extern "C" {
int __wrap_prlimit(pid_t pid, int resource, const struct rlimit* new_limit, struct rlimit* old_limit)
{
    return g_process_control_mock->prlimit(pid, resource, new_limit, old_limit);
}

int __wrap_kill(pid_t pid, int sig)
{
    return g_process_control_mock->kill(pid, sig);
}
}

// NOLINTEND

namespace
{

using OsResult = score::cpp::expected<std::int32_t, score::os::Error>;

OsResult OsSuccess(std::int32_t value = 0)
{
    return OsResult{value};
}

OsResult OsError(std::int32_t error_number)
{
    return score::cpp::make_unexpected(score::os::Error::createFromErrno(error_number));
}

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

class QnxSpawnProcessStarterTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        RecordProperty("TestType", "interface-test");
        RecordProperty("DerivationTechnique", "equivalence-classes");

        g_process_control_mock = std::make_unique<StrictMock<ProcessControlMock>>();

        auto spawn = std::make_unique<NiceMock<score::os::SpawnMock>>();
        auto secpol = std::make_unique<StrictMock<score::os::qnx::MockSecpol>>();
        auto qnx_unistd = std::make_unique<StrictMock<score::os::qnx::MockUnistd>>();
        auto fcntl = std::make_unique<NiceMock<score::os::FcntlMock>>();
        auto unistd = std::make_unique<NiceMock<score::os::UnistdMock>>();
        spawn_ = spawn.get();
        secpol_ = secpol.get();
        qnx_unistd_ = qnx_unistd.get();
        fcntl_ = fcntl.get();
        unistd_ = unistd.get();

        ForwardToRealImplementations();

        starter_ = std::make_unique<SpawnProcessStarter>(
            std::move(spawn), std::move(secpol), std::move(qnx_unistd), std::move(fcntl), std::move(unistd));

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
        starter_.reset();
        g_process_control_mock.reset();
    }

    /// @brief Forwards the attribute, file action and working directory functions to the real implementations by
    /// default, so the resulting spawn attributes can be read back. Spawning itself is never forwarded.
    void ForwardToRealImplementations()
    {
        ON_CALL(*spawn_, posix_spawnattr_init).WillByDefault([this](posix_spawnattr_t* attr) {
            return real_spawn_.posix_spawnattr_init(attr);
        });
        ON_CALL(*spawn_, posix_spawnattr_destroy).WillByDefault([this](posix_spawnattr_t* attr) {
            return real_spawn_.posix_spawnattr_destroy(attr);
        });
        ON_CALL(*spawn_, posix_spawnattr_setpgroup).WillByDefault([this](posix_spawnattr_t* attr, pid_t pgroup) {
            return real_spawn_.posix_spawnattr_setpgroup(attr, pgroup);
        });
        ON_CALL(*spawn_, posix_spawnattr_setschedpolicy)
            .WillByDefault([this](posix_spawnattr_t* attr, std::int32_t policy) {
                return real_spawn_.posix_spawnattr_setschedpolicy(attr, policy);
            });
        ON_CALL(*spawn_, posix_spawnattr_setschedparam)
            .WillByDefault([this](posix_spawnattr_t* attr, const sched_param* param) {
                return real_spawn_.posix_spawnattr_setschedparam(attr, param);
            });
        ON_CALL(*spawn_, pthread_spawnattr_setrunmask_np)
            .WillByDefault([this](posix_spawnattr_t* attr, std::uint64_t runmask) {
                return real_spawn_.pthread_spawnattr_setrunmask_np(attr, runmask);
            });
        ON_CALL(*spawn_, posix_spawnattr_setcred).WillByDefault([this](posix_spawnattr_t* attr, uid_t uid, gid_t gid) {
            return real_spawn_.posix_spawnattr_setcred(attr, uid, gid);
        });
        ON_CALL(*spawn_, posix_spawnattr_setstackmax)
            .WillByDefault([this](posix_spawnattr_t* attr, std::uint32_t size) {
                return real_spawn_.posix_spawnattr_setstackmax(attr, size);
            });
        ON_CALL(*spawn_, posix_spawnattr_setcwd_np).WillByDefault([this](posix_spawnattr_t* attr, std::int32_t fd) {
            return real_spawn_.posix_spawnattr_setcwd_np(attr, fd);
        });
        ON_CALL(*spawn_, posix_spawnattr_getxflags)
            .WillByDefault([this](const posix_spawnattr_t* attr, std::uint32_t* flags) {
                return real_spawn_.posix_spawnattr_getxflags(attr, flags);
            });
        ON_CALL(*spawn_, posix_spawnattr_setxflags).WillByDefault([this](posix_spawnattr_t* attr, std::uint32_t flags) {
            return real_spawn_.posix_spawnattr_setxflags(attr, flags);
        });
        ON_CALL(*spawn_, posix_spawn_file_actions_init).WillByDefault([this](posix_spawn_file_actions_t* actions) {
            return real_spawn_.posix_spawn_file_actions_init(actions);
        });
        ON_CALL(*spawn_, posix_spawn_file_actions_destroy).WillByDefault([this](posix_spawn_file_actions_t* actions) {
            return real_spawn_.posix_spawn_file_actions_destroy(actions);
        });
        ON_CALL(*spawn_, posix_spawn_file_actions_adddup2)
            .WillByDefault([this](posix_spawn_file_actions_t* actions, std::int32_t fd, std::int32_t new_fd) {
                return real_spawn_.posix_spawn_file_actions_adddup2(actions, fd, new_fd);
            });
        ON_CALL(*spawn_, posix_spawn).WillByDefault(Return(OsError(ENOSYS)));

        ON_CALL(*fcntl_, open(_, _)).WillByDefault([this](const char* path, score::os::Fcntl::Open flags) {
            return real_fcntl_.open(path, flags);
        });
        ON_CALL(*unistd_, close).WillByDefault([this](std::int32_t fd) {
            return real_unistd_->close(fd);
        });
    }

    /// @brief Expects a successful posix_spawn that captures the spawn attributes.
    void ExpectSpawnSucceeds()
    {
        EXPECT_CALL(*spawn_, posix_spawn)
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
                return OsSuccess();
            });
    }

    void ExpectHeldProcessConfigured()
    {
        EXPECT_CALL(*qnx_unistd_, setgroupspid(_, _, kChildPid)).WillOnce(Return(OsSuccess()));
        EXPECT_CALL(*g_process_control_mock, prlimit(kChildPid, _, _, nullptr))
            .Times(AnyNumber())
            .WillRepeatedly(Return(0));
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
        return starter_->startProcess(pid_, config_, comms_fd_, comms_);
    }

    static constexpr pid_t kChildPid = 4711;
    static constexpr int kShmFd = 123;

    score::os::SpawnImpl real_spawn_{};
    score::os::FcntlImpl real_fcntl_{};
    std::unique_ptr<score::os::Unistd> real_unistd_{score::os::Unistd::Default()};

    score::os::SpawnMock* spawn_{nullptr};             // owned by starter_
    score::os::qnx::MockSecpol* secpol_{nullptr};      // owned by starter_
    score::os::qnx::MockUnistd* qnx_unistd_{nullptr};  // owned by starter_
    score::os::FcntlMock* fcntl_{nullptr};             // owned by starter_
    score::os::UnistdMock* unistd_{nullptr};           // owned by starter_
    std::unique_ptr<SpawnProcessStarter> starter_;

    configuration::ComponentConfig config_ = {};
    ProcessID pid_{};
    int comms_fd_{-1};
    IpcCommsP comms_;
    CapturedSpawn captured_;

    alignas(IpcCommsSync) std::array<std::byte, sizeof(IpcCommsSync)> test_buffer_;
};

TEST_F(QnxSpawnProcessStarterTest, spawnsHeldProcessWithConfiguredAttributes)
{
    RecordProperty(
        "Description",
        "Verify that the process is spawned held, with the configured process group, scheduling, "
        "credentials, working directory and arguments, and then continued");

    ExpectSpawnSucceeds();
    ExpectHeldProcessConfigured();
    EXPECT_CALL(*g_process_control_mock, kill(kChildPid, SIGCONT)).WillOnce(Return(0));

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

TEST_F(QnxSpawnProcessStarterTest, affinityIsAppliedWhenConfigured)
{
    RecordProperty("Description", "Verify that the runmask is set if an affinity mask is configured");

    config_.deployment_config.sandbox.affinity_mask = 0x5U;

    ExpectSpawnSucceeds();
    ExpectHeldProcessConfigured();
    EXPECT_CALL(*g_process_control_mock, kill(kChildPid, SIGCONT)).WillOnce(Return(0));

    EXPECT_EQ(StartProcess(), OsalReturnType::kSuccess);
    EXPECT_NE(captured_.xflags & static_cast<std::uint32_t>(POSIX_SPAWN_EXPLICIT_CPU), 0U);
    EXPECT_EQ(captured_.runmask, 0x5U);
}

TEST_F(QnxSpawnProcessStarterTest, workingDirIsOpenedAndClosed)
{
    RecordProperty(
        "Description",
        "Verify that the working directory is opened as close-on-exec directory, passed to the spawn attributes and "
        "closed afterwards");

    constexpr std::int32_t kCwdFd = 42;
    const auto expected_flags =
        score::os::Fcntl::Open::kReadOnly | score::os::Fcntl::Open::kDirectory | score::os::Fcntl::Open::kCloseOnExec;

    EXPECT_CALL(*fcntl_, open(StrEq("/"), expected_flags)).WillOnce(Return(OsSuccess(kCwdFd)));
    EXPECT_CALL(*spawn_, posix_spawnattr_setcwd_np(NotNull(), kCwdFd)).WillOnce(Return(OsSuccess()));
    EXPECT_CALL(*unistd_, close(kCwdFd)).WillOnce(Return(score::cpp::expected_blank<score::os::Error>{}));
    ExpectSpawnSucceeds();
    ExpectHeldProcessConfigured();
    EXPECT_CALL(*g_process_control_mock, kill(kChildPid, SIGCONT)).WillOnce(Return(0));

    EXPECT_EQ(StartProcess(), OsalReturnType::kSuccess);
}

TEST_F(QnxSpawnProcessStarterTest, commsFdIsDuplicatedAndPidStoredBeforeContinue)
{
    RecordProperty(
        "Description",
        "Verify that the comms fd is duplicated to sync_fd and that the pid is stored in the shared memory "
        "block before the held process is continued");

    IpcCommsSync* block = StubComms();

    EXPECT_CALL(*spawn_, posix_spawn_file_actions_adddup2(_, kShmFd, IpcCommsSync::sync_fd))
        .WillOnce(Return(OsSuccess()));
    ExpectSpawnSucceeds();
    ExpectHeldProcessConfigured();
    EXPECT_CALL(*g_process_control_mock, kill(kChildPid, SIGCONT)).WillOnce([block](pid_t, int) {
        EXPECT_EQ(block->pid_, kChildPid);
        return 0;
    });

    EXPECT_EQ(StartProcess(), OsalReturnType::kSuccess);
    EXPECT_EQ(block->pid_, kChildPid);
}

TEST_F(QnxSpawnProcessStarterTest, noCommsDoesNotDuplicateFd)
{
    RecordProperty("Description", "Verify that no fd is duplicated for a process without comms");

    EXPECT_CALL(*spawn_, posix_spawn_file_actions_adddup2).Times(0);
    ExpectSpawnSucceeds();
    ExpectHeldProcessConfigured();
    EXPECT_CALL(*g_process_control_mock, kill(kChildPid, SIGCONT)).WillOnce(Return(0));

    EXPECT_EQ(StartProcess(), OsalReturnType::kSuccess);
}

TEST_F(QnxSpawnProcessStarterTest, supplementaryGroupsAreCleared)
{
    RecordProperty(
        "Description", "Verify that the supplementary groups are explicitly cleared when none are configured");

    ExpectSpawnSucceeds();
    EXPECT_CALL(*qnx_unistd_, setgroupspid(0, nullptr, kChildPid)).WillOnce(Return(OsSuccess()));
    EXPECT_CALL(*g_process_control_mock, kill(kChildPid, SIGCONT)).WillOnce(Return(0));

    EXPECT_EQ(StartProcess(), OsalReturnType::kSuccess);
}

TEST_F(QnxSpawnProcessStarterTest, groupsAndLimitsAreAppliedBeforeContinue)
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
        EXPECT_CALL(*qnx_unistd_, setgroupspid(3, NotNull(), kChildPid))
            .WillOnce([&sgids](std::int32_t n, const gid_t* groups, pid_t) {
                EXPECT_TRUE(std::equal(groups, groups + n, sgids.begin()));
                return OsSuccess();
            });
        EXPECT_CALL(
            *g_process_control_mock,
            prlimit(
                kChildPid,
                RLIMIT_NOFILE,
                AllOf(Field(&rlimit::rlim_cur, 64U), Field(&rlimit::rlim_max, 128U)),
                nullptr))
            .WillOnce(Return(0));
        EXPECT_CALL(*g_process_control_mock, prlimit(kChildPid, RLIMIT_STACK, _, nullptr)).WillOnce(Return(0));
        EXPECT_CALL(*g_process_control_mock, kill(kChildPid, SIGCONT)).WillOnce(Return(0));
    }

    EXPECT_EQ(StartProcess(), OsalReturnType::kSuccess);
    EXPECT_NE(captured_.xflags & static_cast<std::uint32_t>(POSIX_SPAWN_SETSTACKMAX), 0U);
    EXPECT_EQ(captured_.stackmax, 65536U);
}

TEST_F(QnxSpawnProcessStarterTest, prlimitFailureKillsHeldProcess)
{
    RecordProperty(
        "Description", "Verify that the held process is killed but reported as started if applying a limit fails");

    config_.deployment_config.sandbox.rlimits = {
        {RLIMIT_CPU, {500U, 500U}},
    };

    ExpectSpawnSucceeds();
    EXPECT_CALL(*qnx_unistd_, setgroupspid).WillOnce(Return(OsSuccess()));
    EXPECT_CALL(*g_process_control_mock, prlimit).WillOnce(SetErrnoAndReturn(EPERM, -1));
    EXPECT_CALL(*g_process_control_mock, kill(kChildPid, SIGKILL)).WillOnce(Return(0));
    EXPECT_CALL(*g_process_control_mock, kill(kChildPid, SIGCONT)).Times(0);

    EXPECT_EQ(StartProcess(), OsalReturnType::kSuccess);
    EXPECT_EQ(pid_, kChildPid);
}

TEST_F(QnxSpawnProcessStarterTest, setgroupspidFailureKillsHeldProcess)
{
    RecordProperty(
        "Description",
        "Verify that the held process is killed but reported as started if setting the supplementary groups fails");

    ExpectSpawnSucceeds();
    EXPECT_CALL(*qnx_unistd_, setgroupspid).WillOnce(Return(OsError(EPERM)));
    EXPECT_CALL(*g_process_control_mock, kill(kChildPid, SIGKILL)).WillOnce(Return(0));
    EXPECT_CALL(*g_process_control_mock, kill(kChildPid, SIGCONT)).Times(0);

    EXPECT_EQ(StartProcess(), OsalReturnType::kSuccess);
    EXPECT_EQ(pid_, kChildPid);
}

TEST_F(QnxSpawnProcessStarterTest, continueFailureKillsHeldProcess)
{
    RecordProperty(
        "Description", "Verify that the held process is killed but reported as started if it cannot be continued");

    ExpectSpawnSucceeds();
    ExpectHeldProcessConfigured();
    EXPECT_CALL(*g_process_control_mock, kill(kChildPid, SIGCONT)).WillOnce(SetErrnoAndReturn(EPERM, -1));
    EXPECT_CALL(*g_process_control_mock, kill(kChildPid, SIGKILL)).WillOnce(Return(0));

    EXPECT_EQ(StartProcess(), OsalReturnType::kSuccess);
    EXPECT_EQ(pid_, kChildPid);
}

TEST_F(QnxSpawnProcessStarterTest, spawnFailureReturnsFail)
{
    RecordProperty("Description", "Verify that startProcess fails if posix_spawn fails");

    EXPECT_CALL(*spawn_, posix_spawn).WillOnce(Return(OsError(ENOENT)));
    EXPECT_CALL(*qnx_unistd_, setgroupspid).Times(0);
    EXPECT_CALL(*g_process_control_mock, kill).Times(0);

    EXPECT_EQ(StartProcess(), OsalReturnType::kFail);
}

TEST_F(QnxSpawnProcessStarterTest, attributeInitFailureReturnsFail)
{
    RecordProperty("Description", "Verify that startProcess fails if the spawn attributes cannot be initialized");

    EXPECT_CALL(*spawn_, posix_spawnattr_init).WillOnce(Return(OsError(ENOMEM)));
    EXPECT_CALL(*spawn_, posix_spawnattr_destroy).Times(0);
    EXPECT_CALL(*spawn_, posix_spawn).Times(0);

    EXPECT_EQ(StartProcess(), OsalReturnType::kFail);
}

TEST_F(QnxSpawnProcessStarterTest, fileActionsInitFailureReturnsFail)
{
    RecordProperty(
        "Description",
        "Verify that startProcess fails if the file actions cannot be initialized and that the already initialized "
        "spawn attributes are destroyed");

    EXPECT_CALL(*spawn_, posix_spawn_file_actions_init).WillOnce(Return(OsError(ENOMEM)));
    EXPECT_CALL(*spawn_, posix_spawn_file_actions_destroy).Times(0);
    EXPECT_CALL(*spawn_, posix_spawnattr_destroy).Times(1);
    EXPECT_CALL(*spawn_, posix_spawn).Times(0);

    EXPECT_EQ(StartProcess(), OsalReturnType::kFail);
}

TEST_F(QnxSpawnProcessStarterTest, credentialsFailureReturnsFail)
{
    RecordProperty("Description", "Verify that startProcess fails if the credentials cannot be set");

    EXPECT_CALL(*spawn_, posix_spawnattr_setcred(_, 11U, 13U)).WillOnce(Return(OsError(EINVAL)));
    EXPECT_CALL(*spawn_, posix_spawn).Times(0);

    EXPECT_EQ(StartProcess(), OsalReturnType::kFail);
}

TEST_F(QnxSpawnProcessStarterTest, invalidWorkingDirReturnsFail)
{
    RecordProperty("Description", "Verify that startProcess fails if the working directory cannot be opened");

    EXPECT_CALL(*fcntl_, open(_, _)).WillOnce(Return(OsError(ENOENT)));
    EXPECT_CALL(*unistd_, close).Times(0);
    EXPECT_CALL(*spawn_, posix_spawn).Times(0);

    EXPECT_EQ(StartProcess(), OsalReturnType::kFail);
}

TEST_F(QnxSpawnProcessStarterTest, securityPolicyIsApplied)
{
    RecordProperty("Description", "Verify that the configured security policy is set as type in the spawn attributes");

    config_.deployment_config.sandbox.security_policy = "app_t";

    EXPECT_CALL(*secpol_, secpol_posix_spawnattr_settypeid(nullptr, NotNull(), StrEq("app_t"), SECPOL_TYPE_NAME))
        .WillOnce(Return(OsSuccess()));
    ExpectSpawnSucceeds();
    ExpectHeldProcessConfigured();
    EXPECT_CALL(*g_process_control_mock, kill(kChildPid, SIGCONT)).WillOnce(Return(0));

    EXPECT_EQ(StartProcess(), OsalReturnType::kSuccess);
}

TEST_F(QnxSpawnProcessStarterTest, securityPolicyNotSupportedIsIgnored)
{
    RecordProperty("Description", "Verify that a missing security policy support (ENOTSUP) is ignored");

    config_.deployment_config.sandbox.security_policy = "app_t";

    EXPECT_CALL(*secpol_, secpol_posix_spawnattr_settypeid).WillOnce(Return(OsError(ENOTSUP)));
    ExpectSpawnSucceeds();
    ExpectHeldProcessConfigured();
    EXPECT_CALL(*g_process_control_mock, kill(kChildPid, SIGCONT)).WillOnce(Return(0));

    EXPECT_EQ(StartProcess(), OsalReturnType::kSuccess);
}

TEST_F(QnxSpawnProcessStarterTest, securityPolicyFailureReturnsFail)
{
    RecordProperty("Description", "Verify that startProcess fails if the security policy type cannot be set");

    config_.deployment_config.sandbox.security_policy = "unknown_t";

    EXPECT_CALL(*secpol_, secpol_posix_spawnattr_settypeid).WillOnce(Return(OsError(EINVAL)));
    EXPECT_CALL(*spawn_, posix_spawn).Times(0);

    EXPECT_EQ(StartProcess(), OsalReturnType::kFail);
}
