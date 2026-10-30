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
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sched.h>
#include <array>
#include <cstring>
#include <string>
#include <vector>

#include "score/mw/launch_manager/osal/start_process.hpp"
#include "tests/utils/mocks/mock_syscalls.hpp"

using namespace testing;

using namespace score::mw::lifecycle::internal::osal;
using namespace score::mw::lifecycle::internal;

namespace
{

// The sysexit mock can throw this to interrupt execution
struct SysExitException
{
};

// Define a matcher that checks a null-terminated char** against a vector of strings
MATCHER_P(ArgvMatches, expected_args, "")
{
    for (size_t i = 0; i < expected_args.size(); ++i)
    {
        // Check if the argument is null prematurely, or if the string doesn't match
        if (arg[i] == nullptr || std::string(arg[i]) != expected_args[i])
        {
            *result_listener << "mismatch at index " << i << " (expected: \"" << expected_args[i] << "\", got: \""
                             << (arg[i] ? arg[i] : "NULL") << "\")";
            return false;
        }
    }
    // execve argv must be null-terminated; ensure the last expected element is followed by NULL
    return arg[expected_args.size()] == nullptr;
}

MATCHER_P(CArrayMatches, expected, "")
{
    for (size_t i = 0; i < expected.size(); ++i)
    {
        if (arg[i] != expected[i])
        {
            *result_listener << "mismatch at index " << i << " (expected: " << expected[i] << ", got: " << arg[i]
                             << ")";
            return false;
        }
    }
    return true;
}

}  // namespace

class StartProcessTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        RecordProperty("TestType", "interface-test");
        RecordProperty("DerivationTechnique", "equivalence-classes");

        g_syscall_mock = std::make_unique<SyscallMock>();

        // Used by logging framework
        ON_CALL(*g_syscall_mock, access).WillByDefault(Invoke(__real_access));
        ON_CALL(*g_syscall_mock, fcntl).WillByDefault(Invoke(__real_fcntl));
        EXPECT_CALL(*g_syscall_mock, access).Times(AnyNumber());
        EXPECT_CALL(*g_syscall_mock, fcntl).Times(AnyNumber());

        EXPECT_CALL(*g_syscall_mock, getpid).Times(AnyNumber()).WillRepeatedly(Return(forked_pid));

        config_.name = "TestComponent";
        config_.component_properties.binary_name = "TestProcess";
        config_.component_properties.application_profile.application_type = configuration::ApplicationType::Native;
        config_.deployment_config.process_command = configuration::ProcessCommand{"/bin/TestProcess", {}};
        config_.deployment_config.working_dir = "/tmp";
        config_.deployment_config.sandbox.security_policy = std::nullopt;
        config_.deployment_config.sandbox.scheduling_priority = 1;
        config_.deployment_config.sandbox.scheduling_policy = SCHED_RR;
        config_.deployment_config.sandbox.gid = 1;
        config_.deployment_config.sandbox.uid = 1;
        config_.deployment_config.sandbox.supplementary_group_ids = {};
    }

    void TearDown() override
    {
        comms_.reset();
        g_syscall_mock.reset();
    }

    void ExpectChildProcessStarts()
    {
        EXPECT_CALL(*g_syscall_mock, fork).WillOnce(Return(0));
    }

    void ExpectSuccessfulSchedulingAndSecurity()
    {
        EXPECT_CALL(*g_syscall_mock, setpgid).Times(AnyNumber()).WillRepeatedly(Return(0));
        EXPECT_CALL(*g_syscall_mock, sched_setscheduler).Times(AnyNumber()).WillRepeatedly(Return(0));
        EXPECT_CALL(*g_syscall_mock, setgid).Times(AnyNumber()).WillRepeatedly(Return(0));
        EXPECT_CALL(*g_syscall_mock, setuid).Times(AnyNumber()).WillRepeatedly(Return(0));
        EXPECT_CALL(*g_syscall_mock, setgroups).Times(AnyNumber()).WillRepeatedly(Return(0));
    }

    void ExpectSuccessfulChdir()
    {
        EXPECT_CALL(*g_syscall_mock, chdir).Times(AnyNumber()).WillRepeatedly(Return(0));
    }

    /// @brief Sets up a shared memory block for a reporting process, as ProcessLauncher::setupComms() does.
    IpcCommsSync* StubComms()
    {
        std::memset(test_buffer.data(), 0, test_buffer.size());
        auto* block = reinterpret_cast<IpcCommsSync*>(test_buffer.data());
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

    static constexpr int kShmFd = 123;
    const pid_t forked_pid = 23;

    configuration::ComponentConfig config_ = {};
    ProcessID pid_{};
    int comms_fd_{-1};
    IpcCommsP comms_;

    alignas(IpcCommsSync) std::array<std::byte, sizeof(IpcCommsSync)> test_buffer;
};

TEST_F(StartProcessTest, forkFailed)
{
    RecordProperty("Description", "Test that startProcess returns a failure if fork fails");

    EXPECT_CALL(*g_syscall_mock, fork).WillOnce(Return(-1));

    EXPECT_EQ(StartProcess(), OsalReturnType::kFail);
}

TEST_F(StartProcessTest, forkSucceedsInParent)
{
    RecordProperty("Description", "Test that startProcess returns the pid of the child process in the parent");

    EXPECT_CALL(*g_syscall_mock, fork).WillOnce(Return(forked_pid));
    EXPECT_CALL(*g_syscall_mock, execve).Times(0);

    EXPECT_EQ(StartProcess(), OsalReturnType::kSuccess);
    EXPECT_EQ(pid_, forked_pid);
}

TEST_F(StartProcessTest, handleCommsFailsFnctl)
{
    RecordProperty(
        "Description", "Verify that the forked process exits if setting up comms fails due to a syscall failure");

    StubComms();
    ExpectChildProcessStarts();
    EXPECT_CALL(*g_syscall_mock, fcntl).WillOnce(Return(-1));

    EXPECT_CALL(*g_syscall_mock, sysexit(EXIT_FAILURE)).WillOnce(Throw(SysExitException{}));

    EXPECT_THROW(static_cast<void>(StartProcess()), SysExitException);
}

class SetSchedulingAndSecurityTest : public StartProcessTest
{
  protected:
    void SetUp() override
    {
        StartProcessTest::SetUp();

        ExpectChildProcessStarts();
        ExpectSuccessfulChdir();
        EXPECT_CALL(*g_syscall_mock, execve).Times(AtMost(1));
    }
};

TEST_F(SetSchedulingAndSecurityTest, setpgidFails)
{
    RecordProperty("Description", "Verify that the forked process exits if setting pgid fails");

    EXPECT_CALL(*g_syscall_mock, setpgid).WillOnce(Return(-1));
    EXPECT_CALL(*g_syscall_mock, sched_setscheduler).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setgid).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setuid).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setgroups).WillOnce(Return(0));

    EXPECT_CALL(*g_syscall_mock, sysexit(EXIT_FAILURE)).WillOnce(Throw(SysExitException{}));

    EXPECT_THROW(static_cast<void>(StartProcess()), SysExitException);
}

TEST_F(SetSchedulingAndSecurityTest, setgidFails)
{
    RecordProperty("Description", "Verify that the forked process exits if setting gid fails");

    EXPECT_CALL(*g_syscall_mock, setpgid).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, sched_setscheduler).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setgid).WillOnce(Return(-1));
    EXPECT_CALL(*g_syscall_mock, setuid).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setgroups).WillOnce(Return(0));

    EXPECT_CALL(*g_syscall_mock, sysexit(EXIT_FAILURE)).WillOnce(Throw(SysExitException{}));

    EXPECT_THROW(static_cast<void>(StartProcess()), SysExitException);
}

TEST_F(SetSchedulingAndSecurityTest, setuidFails)
{
    RecordProperty("Description", "Verify that the forked process exits if setting uid fails");

    EXPECT_CALL(*g_syscall_mock, setpgid).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, sched_setscheduler).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setgid).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setuid).WillOnce(Return(-1));
    EXPECT_CALL(*g_syscall_mock, setgroups).WillOnce(Return(0));

    EXPECT_CALL(*g_syscall_mock, sysexit(EXIT_FAILURE)).WillOnce(Throw(SysExitException{}));

    EXPECT_THROW(static_cast<void>(StartProcess()), SysExitException);
}

TEST_F(SetSchedulingAndSecurityTest, setschedFails)
{
    RecordProperty("Description", "Verify that the forked process exits if setting the scheduler fails");

    EXPECT_CALL(*g_syscall_mock, setpgid).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, sched_setscheduler).WillOnce(Return(-1));
    EXPECT_CALL(*g_syscall_mock, setgid).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setuid).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setgroups).WillOnce(Return(0));

    EXPECT_CALL(*g_syscall_mock, sysexit(EXIT_FAILURE)).WillOnce(Throw(SysExitException{}));

    EXPECT_THROW(static_cast<void>(StartProcess()), SysExitException);
}

TEST_F(SetSchedulingAndSecurityTest, setschedPassesConfiguredPriority)
{
    RecordProperty(
        "Description",
        "Verify that the configured priority is passed unchanged, as it was clamped when loading the configuration");

    config_.deployment_config.sandbox.scheduling_policy = SCHED_FIFO;
    config_.deployment_config.sandbox.scheduling_priority = 42;

    EXPECT_CALL(*g_syscall_mock, setpgid).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setgid).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setuid).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setgroups).WillOnce(Return(0));

    EXPECT_CALL(*g_syscall_mock, sched_setscheduler(0, SCHED_FIFO, Field(&sched_param::sched_priority, Eq(42))))
        .WillOnce(Return(0));

    static_cast<void>(StartProcess());  // No return from forked process
}

TEST_F(SetSchedulingAndSecurityTest, setgroupsFails)
{
    RecordProperty("Description", "Verify that if setting supplementary gids fails, the forked process exits");

    config_.deployment_config.sandbox.supplementary_group_ids = {1, 2, 3};

    EXPECT_CALL(*g_syscall_mock, setpgid).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setgid).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setuid).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, sched_setscheduler).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setgroups).WillOnce(Return(-1));

    EXPECT_CALL(*g_syscall_mock, sysexit(EXIT_FAILURE)).WillOnce(Throw(SysExitException{}));

    EXPECT_THROW(static_cast<void>(StartProcess()), SysExitException);
}

TEST_F(SetSchedulingAndSecurityTest, setaffinityFails)
{
    RecordProperty("Description", "Verify that the forked process exits if setting the CPU affinity fails");

    config_.deployment_config.sandbox.affinity_mask = 0x1U;

    EXPECT_CALL(*g_syscall_mock, setpgid).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, sched_setscheduler).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setaffinity(0x1U)).WillOnce(SetErrnoAndReturn(EINVAL, -1));
    EXPECT_CALL(*g_syscall_mock, setgid).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setuid).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setgroups).WillOnce(Return(0));

    EXPECT_CALL(*g_syscall_mock, sysexit(EXIT_FAILURE)).WillOnce(Throw(SysExitException{}));

    EXPECT_THROW(static_cast<void>(StartProcess()), SysExitException);
}

TEST_F(SetSchedulingAndSecurityTest, setaffinityIgnore)
{
    RecordProperty("Description", "Verify that the CPU affinity is not changed if no affinity mask is configured");

    config_.deployment_config.sandbox.affinity_mask = std::nullopt;

    EXPECT_CALL(*g_syscall_mock, setpgid).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, sched_setscheduler).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setgid).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setuid).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setaffinity).Times(0);
    EXPECT_CALL(*g_syscall_mock, setgroups).WillOnce(Return(0));

    static_cast<void>(StartProcess());  // No return from forked process
}

TEST_F(SetSchedulingAndSecurityTest, supplementaryGroupsAreCleared)
{
    RecordProperty(
        "Description", "Verify that supplementary group IDs are explicitly cleared when none are configured");

    config_.deployment_config.sandbox.supplementary_group_ids = {};

    ExpectSuccessfulSchedulingAndSecurity();
    EXPECT_CALL(*g_syscall_mock, setgroups(0, _)).WillOnce(Return(0));

    static_cast<void>(StartProcess());
}

TEST_F(StartProcessTest, chdirFails)
{
    RecordProperty("Description", "Verify that the forked process exits if changing the working dir fails");

    ExpectChildProcessStarts();
    ExpectSuccessfulSchedulingAndSecurity();
    EXPECT_CALL(*g_syscall_mock, chdir).WillOnce(Return(-1));

    EXPECT_CALL(*g_syscall_mock, sysexit(EXIT_FAILURE)).WillOnce(Throw(SysExitException{}));

    EXPECT_THROW(static_cast<void>(StartProcess()), SysExitException);
}

TEST_F(StartProcessTest, securityPolicyIsIgnored)
{
    RecordProperty(
        "Description", "Verify that a configured security policy does not prevent the process from starting on Linux");

    config_.deployment_config.sandbox.security_policy = "security";

    ExpectChildProcessStarts();
    ExpectSuccessfulSchedulingAndSecurity();
    ExpectSuccessfulChdir();
    EXPECT_CALL(*g_syscall_mock, execve).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, sysexit).Times(0);

    EXPECT_EQ(StartProcess(), OsalReturnType::kSuccess);
}

TEST_F(StartProcessTest, setRLimitFails)
{
    RecordProperty("Description", "Verify that the forked process exits if setting a limit fails");

    config_.deployment_config.sandbox.rlimits = {
        {RLIMIT_CPU, {500, 500}},
    };

    ExpectChildProcessStarts();
    ExpectSuccessfulSchedulingAndSecurity();
    ExpectSuccessfulChdir();
    EXPECT_CALL(*g_syscall_mock, setrlimit).WillOnce(Return(-1));

    EXPECT_CALL(*g_syscall_mock, sysexit(EXIT_FAILURE)).WillOnce(Throw(SysExitException{}));

    EXPECT_THROW(static_cast<void>(StartProcess()), SysExitException);
}

TEST_F(StartProcessTest, noRlimitsConfiguredSkipsSetrlimit)
{
    RecordProperty("Description", "Verify that setrlimit is not called when no rlimits are configured");

    ExpectChildProcessStarts();
    ExpectSuccessfulSchedulingAndSecurity();
    ExpectSuccessfulChdir();
    EXPECT_CALL(*g_syscall_mock, setrlimit).Times(0);
    EXPECT_CALL(*g_syscall_mock, execve).Times(AtMost(1));

    static_cast<void>(StartProcess());  // No return from forked process
}

TEST_F(StartProcessTest, setRLimitZeroIsApplied)
{
    RecordProperty("Description", "Verify that a configured rlimit of 0 is applied via setrlimit");

    config_.deployment_config.sandbox.rlimits = {
        {RLIMIT_CORE, {0, 0}},
    };

    ExpectChildProcessStarts();
    ExpectSuccessfulSchedulingAndSecurity();
    ExpectSuccessfulChdir();
    EXPECT_CALL(
        *g_syscall_mock, setrlimit(RLIMIT_CORE, AllOf(Field(&rlimit::rlim_cur, 0U), Field(&rlimit::rlim_max, 0U))))
        .WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, execve).Times(AtMost(1));

    static_cast<void>(StartProcess());  // No return from forked process
}

TEST_F(StartProcessTest, setRLimitPassesPreResolvedValuesUnchanged)
{
    RecordProperty(
        "Description",
        "Verify that applyResourceLimits() passes the resource and limit through to setrlimit() unchanged.");

    config_.deployment_config.sandbox.rlimits = {
        {RLIMIT_FSIZE, {1024U, RLIM_INFINITY}},
    };

    ExpectChildProcessStarts();
    ExpectSuccessfulSchedulingAndSecurity();
    ExpectSuccessfulChdir();
    EXPECT_CALL(
        *g_syscall_mock,
        setrlimit(
            RLIMIT_FSIZE,
            AllOf(Field(&rlimit::rlim_cur, static_cast<rlim_t>(1024U)), Field(&rlimit::rlim_max, RLIM_INFINITY))))
        .WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, execve).Times(AtMost(1));

    static_cast<void>(StartProcess());  // No return from forked process
}

TEST_F(StartProcessTest, rlimitsAppliedBeforePrivilegeDrop)
{
    RecordProperty(
        "Description",
        "Regression guard for the ordering defect where rlimits were applied after setuid()/setgid(): raising a "
        "hard limit requires privilege, so setrlimit must be called before setuid.");

    config_.deployment_config.sandbox.rlimits = {
        {RLIMIT_NOFILE, {1024, 1024}},
    };

    ExpectChildProcessStarts();
    ExpectSuccessfulChdir();

    EXPECT_CALL(*g_syscall_mock, setpgid).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, sched_setscheduler).WillOnce(Return(0));

    {
        InSequence seq;
        EXPECT_CALL(*g_syscall_mock, setrlimit(RLIMIT_NOFILE, _)).WillOnce(Return(0));
        EXPECT_CALL(*g_syscall_mock, setgid).WillOnce(Return(0));
        EXPECT_CALL(*g_syscall_mock, setgroups(_, _)).WillOnce(Return(0));
        EXPECT_CALL(*g_syscall_mock, setuid).WillOnce(Return(0));
    }

    static_cast<void>(StartProcess());  // No return from forked process
}

TEST_F(StartProcessTest, execveFails)
{
    RecordProperty("Description", "Verify that the forked process exits if execve fails");

    ExpectChildProcessStarts();
    ExpectSuccessfulSchedulingAndSecurity();
    ExpectSuccessfulChdir();
    EXPECT_CALL(*g_syscall_mock, execve).WillOnce(Return(-1));

    EXPECT_CALL(*g_syscall_mock, sysexit(EXIT_FAILURE)).WillOnce(Throw(SysExitException{}));

    EXPECT_THROW(static_cast<void>(StartProcess()), SysExitException);
}

TEST_F(StartProcessTest, startProcessChildSuccess)
{
    RecordProperty("Description", "Verify that when startProcess succeeds, the forked process is configured correctly");

    const int scheduler = SCHED_FIFO;
    const int uid = 11;
    const int gid = 13;
    const std::vector<gid_t> sgids = {1, 2, 3};
    const std::uint64_t mem_limit = 4096;
    const std::uint32_t cpu_limit = 500;
    const std::uint64_t affinity_mask = 0x8000000000000005U;
    const std::string security_policy = "security";
    const std::vector<std::string> args_in = {"-c 2", "--argument yes"};
    const std::vector<std::string> expected_launch_args = {
        config_.deployment_config.process_command.executablePath(), args_in[0], args_in[1]};

    config_.component_properties.application_profile.application_type = configuration::ApplicationType::Reporting;
    config_.deployment_config.sandbox.scheduling_policy = scheduler;
    config_.deployment_config.sandbox.uid = uid;
    config_.deployment_config.sandbox.gid = gid;
    config_.deployment_config.sandbox.supplementary_group_ids = sgids;
    config_.deployment_config.sandbox.rlimits = {
        {RLIMIT_AS, {mem_limit, mem_limit}},
        {RLIMIT_CPU, {cpu_limit, RLIM_INFINITY}},
    };
    config_.deployment_config.sandbox.affinity_mask = affinity_mask;
    config_.deployment_config.sandbox.security_policy = security_policy;
    config_.deployment_config.environmental_variables.add("environment", "yes");
    config_.deployment_config.environmental_variables.add("errors", "no");
    config_.deployment_config.process_command =
        configuration::ProcessCommand{config_.deployment_config.process_command.executablePath(), args_in};

    IpcCommsSync* block = StubComms();

    EXPECT_CALL(*g_syscall_mock, fork).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, getpid).WillRepeatedly(Return(forked_pid));
    EXPECT_CALL(*g_syscall_mock, fcntl).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setpgid(0, forked_pid));
    EXPECT_CALL(*g_syscall_mock, sched_setscheduler(0, scheduler, _)).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setaffinity(affinity_mask)).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setuid(uid)).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setgid(gid)).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, setgroups(sgids.size(), CArrayMatches(sgids))).WillOnce(Return(0));
    EXPECT_CALL(*g_syscall_mock, chdir(StrEq("/tmp"))).WillOnce(Return(0));
    EXPECT_CALL(
        *g_syscall_mock,
        setrlimit(RLIMIT_AS, AllOf(Field(&rlimit::rlim_cur, mem_limit), Field(&rlimit::rlim_max, mem_limit))));
    EXPECT_CALL(
        *g_syscall_mock,
        setrlimit(RLIMIT_CPU, AllOf(Field(&rlimit::rlim_cur, cpu_limit), Field(&rlimit::rlim_max, RLIM_INFINITY))));
    EXPECT_CALL(
        *g_syscall_mock,
        execve(
            StrEq(expected_launch_args[0]),
            ArgvMatches(expected_launch_args),
            config_.deployment_config.environmental_variables.envp()))
        .WillOnce(Return(0));

    EXPECT_CALL(*g_syscall_mock, sysexit).Times(0);  // No failures

    static_cast<void>(StartProcess());  // No return from forked process

    EXPECT_EQ(block->pid_, forked_pid);
}
