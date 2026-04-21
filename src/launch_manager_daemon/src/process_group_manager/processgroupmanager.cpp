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

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <csignal>

#include <process_group_manager/ihealth_monitor_thread.hpp>
#include <process_group_manager/processgroupmanager.hpp>
#include <score/lcm/exec_error_domain.h>
#include <score/lcm/internal/log.hpp>

namespace score
{

namespace lcm
{

namespace internal
{

using namespace score::lcm::internal::osal;

static std::atomic_bool em_cancelled{false};

static void my_signal_handler(int)
{
    em_cancelled.store(true);
}

void ProcessGroupManager::cancel()
{
    my_signal_handler(SIGTERM);
}

ProcessGroupManager::ProcessGroupManager(std::unique_ptr<IHealthMonitorThread> health_monitor,
                                         std::shared_ptr<IRecoveryClient> recovery_client,
                                         std::unique_ptr<score::lcm::IProcessStateNotifier> process_state_notifier)
    : configuration_manager_(),
      process_interface_(),
      process_map_(nullptr),
      worker_threads_(nullptr),
      worker_jobs_(nullptr),
      total_processes_(0U),
      num_process_groups_(0U),
      process_groups_(),
      process_state_notifier_(std::move(process_state_notifier)),
      health_monitor_thread_(std::move(health_monitor)),
      recovery_client_(recovery_client)
{
}

ProcessGroupManager::~ProcessGroupManager() = default;

void ProcessGroupManager::setLaunchManagerConfiguration(const OsProcess* launch_manager_configuration)
{
    if (launch_manager_config_)
    {
        LM_LOG_WARN() << "More than one launch manager configured! (ignoring)";
    }
    else
    {
        launch_manager_config_ = launch_manager_configuration;
    }
}

bool ProcessGroupManager::initialize()
{
    // setup signal handler
    em_cancelled.store(false);
    // RULECHECKER_comment(1, 1, check_union_object, "Union type defined in external library is used.", true)
    struct sigaction action;

    action.sa_handler = my_signal_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGALRM, &action, NULL);
    sigaction(SIGHUP, &action, NULL);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGIO, &action, NULL);
    sigaction(SIGPROF, &action, NULL);
    sigaction(SIGQUIT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGUSR1, &action, NULL);
    sigaction(SIGUSR2, &action, NULL);
    sigaction(SIGVTALRM, &action, NULL);

    if (!initializeProcessGroups())
    {
        return false;
    }

    // Initialize the main loop semaphore
    main_loop_sem_.init(0U, false);

    // Wire up recovery client notification callback
    if (recovery_client_)
    {
        recovery_client_->setNotifyCallback([this]() { nudgeMainLoop(); });
    }

    LM_LOG_DEBUG() << "Process Group initialization done";
    createProcessComponentsObjects();
    initializeGraphNodes();
    if (!health_monitor_thread_->start())
    {
        LM_LOG_ERROR() << "Health monitor thread failed to start";
        return false;
    }

    if (launch_manager_config_ &&
        OsalReturnType::kFail == IProcess::setSchedulingAndSecurity(launch_manager_config_->startup_config_))
    {
        return false;
    }

    return true;
}

void ProcessGroupManager::deinitialize()
{
    // ucm_polling_thread_.stopPolling();
    health_monitor_thread_->stop();

    main_loop_sem_.deinit();

    configuration_manager_.deinitialize();
    process_groups_.clear();

    worker_threads_.reset();
    worker_jobs_.reset();
    process_map_.reset();
}

inline bool ProcessGroupManager::initializeProcessGroups()
{
    bool success = false;

    if (configuration_manager_.initialize())
    {
        auto pg_list = configuration_manager_.getListOfProcessGroups().value_or(nullptr);

        if (pg_list && !pg_list->empty())
        {
            num_process_groups_ = static_cast<uint32_t>(pg_list->size() & 0xFFFFFFFFUL);
            LM_LOG_DEBUG() << num_process_groups_ << "process group(s)";

            success = true;

            for (const auto& pg_name : *pg_list)
            {
                uint32_t num_processes = configuration_manager_.getNumberOfOsProcesses(pg_name).value_or(0);

                if (static_cast<uint64_t>(total_processes_) + num_processes <=
                    static_cast<uint64_t>(score::lcm::internal::ProcessLimits::kMaxProcesses))
                {
                    process_groups_.push_back(std::make_shared<Graph>(num_processes, this));
                    total_processes_ += num_processes;
                }
                else
                {
                    LM_LOG_ERROR() << "Too many processes";
                    success = false;
                    break;
                }
            }
        }
        else
        {
            LM_LOG_ERROR() << "No process groups";
        }
    }
    else
    {
        LM_LOG_ERROR() << "Failed to initialize configuration manager";
    }

    if (success)
    {
        trackers_.resize(process_groups_.size());
        LM_LOG_DEBUG() << "Process groups initialized successfully";
    }
    else
    {
        LM_LOG_ERROR() << "Failed to initialize process groups";
    }

    return success;
}

inline void ProcessGroupManager::createProcessComponentsObjects()
{
    LM_LOG_DEBUG() << "Creating Safe Process Map with" << total_processes_ << "entries";
    process_map_ = std::make_shared<SafeProcessMap>(total_processes_);

    LM_LOG_DEBUG() << "Creating job queue with" << total_processes_ << "entries";
    worker_jobs_ = std::make_shared<JobQueue<ProcessInfoNode>>(total_processes_);

    LM_LOG_DEBUG() << "Creating worker threads...";
    worker_threads_ = std::make_unique<WorkerThread<ProcessInfoNode>>(
        worker_jobs_, static_cast<uint32_t>(ProcessLimits::kNumWorkerThreads));
}

inline void ProcessGroupManager::initializeGraphNodes()
{
    auto pg_list = configuration_manager_.getListOfProcessGroups().value_or(nullptr);

    for (size_t idx = 0U; idx < process_groups_.size(); ++idx)
    {
        process_groups_[idx]->initProcessGroupNodes(
            pg_list->at(idx),
            configuration_manager_.getNumberOfOsProcesses(pg_list->at(idx)).value_or(0U),
            static_cast<uint32_t>(idx & 0xFFFFFFFFUL));
    }

    LM_LOG_DEBUG() << "Graphs initialized";
}

bool ProcessGroupManager::run()
{
    // RULECHECKER_comment(1, 4, check_c_style_cast, "This is the definition provided by the OS and does a C-style
    // cast.", true)
    LM_LOG_DEBUG() << "clock() at run():"
                   // coverity[cert_err33_c_violation:INTENTIONAL] Does not matter if clock() gives a weird value in
                   // debug messages.
                   << (static_cast<double>(clock()) / (static_cast<double>(CLOCKS_PER_SEC) / 1000.0)) << "ms";

    OsHandler os_handler(*process_map_, process_interface_);

    bool result = startInitialTransition();

    if (result)
        while (!em_cancelled.load())
        {
            // Wait for any event source to signal (control requests, recovery, graph completions)
            main_loop_sem_.timedWait(std::chrono::milliseconds(100));

            // Process queued activation requests (enqueued by ActivateRunTarget from any thread)
            processTransitionRequests();

            // Resolve promises for completed transitions (before starting new ones)
            processTransitionResponses();

            for (size_t i = 0; i < process_groups_.size(); ++i)
            {
                processGroupHandler(*process_groups_[i], trackers_[i]);
            }

            recoveryActionHandler();
        }

    allProcessGroupsOff();

    return result;
}

inline bool ProcessGroupManager::startInitialTransition()
{
    bool result = false;
    LM_LOG_DEBUG() << "=============STARTING MAINPG STARTUP STATE============";

    // Initial transition of machine process group
    const ProcessGroupStateID* pg_startup_id = configuration_manager_.getMainPGStartupState().value_or(nullptr);

    if (pg_startup_id)
    {
        machine_process_group_ = getProcessGroup(pg_startup_id->pg_name_);

        if (machine_process_group_)
        {
            result = machine_process_group_->startInitialTransition(*pg_startup_id);
        }
    }
    else
    {
        LM_LOG_ERROR() << "No startup state, exiting from process group manager";
    }
    return result;
}

inline void ProcessGroupManager::allProcessGroupsOff()
{
    // Lambda to wait for process group transitions with a timeout
    auto waitForStateCompletion =
        [](auto& process_groups, GraphState state_to_be_completed, int32_t max_wait_ms) -> bool {
        int32_t counter = max_wait_ms;
        constexpr int32_t sleep_interval_ms = 10;
        for (auto& pg : process_groups)
        {
            while (pg->getState() == state_to_be_completed && counter > 1)
            {
                counter -= sleep_interval_ms;
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_interval_ms));
            }
        }
        return counter > 0;
    };

    // First, cancel any pending transitions
    LM_LOG_DEBUG() << "Cancel all process group transitions";

    for (auto& pg : process_groups_)
    {
        pg->cancel();
    }

    // Wait for cancellation to complete (max 2 seconds)
    LM_LOG_DEBUG() << "Wait for process group cancellations";

    if (!waitForStateCompletion(process_groups_, GraphState::kCancelled, 2000))
    {
        LM_LOG_ERROR() << "NOTE: Cancellation timed out";
    }

    // Start transitioning all process groups to the "Off" state
    LM_LOG_DEBUG() << "Start transitioning process groups to Off state";

    for (auto& pg : process_groups_)
    {
        (void)pg->startTransitionToOffState();
    }

    // Wait for the transition to complete (max 1 second)
    LM_LOG_DEBUG() << "Wait for all process groups to complete the transition";

    if (!waitForStateCompletion(process_groups_, GraphState::kInTransition, 1000))
    {
        LM_LOG_ERROR() << "NOTE: Transition to Off state timed out";
        worker_jobs_->stopQueue(static_cast<std::size_t>(ProcessLimits::kNumWorkerThreads));
        for (auto& pg : process_groups_)
        {
            for (auto& node : pg->getNodes())
            {
                osal::ProcessID pid = node->getPid();
                if (pid > 0)
                {
                    process_interface_.forceTermination(pid);
                }
            }
        }
        while (wait(NULL) != -1 || errno == EINTR)
            ;
    }
}

inline void ProcessGroupManager::processTransitionRequests()
{
    while (auto req_opt = activation_queue_.tryDequeue())
    {
        auto& req = *req_opt;
        auto promise = std::move(req.promise);

        auto pg = machine_process_group_;
        if (nullptr == pg)
        {
            promise.SetError(score::lcm::MakeError(score::lcm::ExecErrc::kInvalidArguments));
            continue;
        }

        LM_LOG_DEBUG() << "ProcessGroupManager: Processing activation request for state" << req.target_state;

        IdentifierHash old_state = pg->getProcessGroupState();
        GraphState graph_state = pg->getState();
        auto& tracker = trackers_[pg->getProcessGroupIndex()];

        if (GraphState::kInTransition == graph_state)
        {
            if (old_state != req.target_state)
            {
                // Cancel existing pending promise if any
                if (tracker.pending && tracker.pending->promise)
                {
                    tracker.pending->promise->SetError(score::lcm::MakeError(score::lcm::ExecErrc::kCancelled));
                }
                tracker.pending = TransitionContext{req.target_state, std::move(promise)};
                pg->setRequestStartTime();
                pg->cancel();
            }
            else
            {
                promise.SetError(score::lcm::MakeError(score::lcm::ExecErrc::kInTransitionToSameState));
            }
        }
        else if (GraphState::kSuccess == graph_state && old_state == req.target_state)
        {
            promise.SetError(score::lcm::MakeError(score::lcm::ExecErrc::kAlreadyInState));
        }
        else
        {
            // Cancel existing pending promise if any
            if (tracker.pending && tracker.pending->promise)
            {
                tracker.pending->promise->SetError(score::lcm::MakeError(score::lcm::ExecErrc::kCancelled));
            }
            tracker.pending = TransitionContext{req.target_state, std::move(promise)};
            pg->setRequestStartTime();
        }
    }
}

static score::lcm::ExecErrc controlClientCodeToExecErrc(ControlClientCode code)
{
    switch (code)
    {
        case ControlClientCode::kSetStateInvalidArguments:
            return score::lcm::ExecErrc::kInvalidArguments;
        case ControlClientCode::kSetStateCancelled:
            return score::lcm::ExecErrc::kCancelled;
        case ControlClientCode::kSetStateFailed:
            return score::lcm::ExecErrc::kFailed;
        case ControlClientCode::kSetStateAlreadyInState:
            return score::lcm::ExecErrc::kAlreadyInState;
        case ControlClientCode::kSetStateTransitionToSameState:
            return score::lcm::ExecErrc::kInTransitionToSameState;
        case ControlClientCode::kFailedUnexpectedTerminationOnEnter:
            return score::lcm::ExecErrc::kFailedUnexpectedTerminationOnEnter;
        case ControlClientCode::kFailedUnexpectedTermination:
            return score::lcm::ExecErrc::kFailedUnexpectedTerminationOnExit;
        default:
            return score::lcm::ExecErrc::kGeneralError;
    }
}

inline void ProcessGroupManager::processTransitionResponses()
{
    for (size_t i = 0; i < process_groups_.size(); ++i)
    {
        auto& pg = process_groups_[i];
        auto& tracker = trackers_[i];

        ControlClientCode event = pg->getPendingEvent();

        if (ControlClientCode::kNotSet != event)
        {
            if (tracker.in_flight && tracker.in_flight->promise)
            {
                if (ControlClientCode::kSetStateSuccess == event)
                {
                    tracker.in_flight->promise->SetValue();
                }
                else
                {
                    score::lcm::ExecErrc errc = controlClientCodeToExecErrc(event);
                    tracker.in_flight->promise->SetError(score::lcm::MakeError(errc));
                }

                LM_LOG_DEBUG() << "ProcessGroupManager: Resolved promise for transition, code"
                               << static_cast<int>(event) << "for PG" << pg->getProcessGroupName();
            }

            pg->clearPendingEvent(event);
            tracker.in_flight.reset();
        }
    }
}

inline void ProcessGroupManager::recoveryActionHandler()
{
    if (!recovery_client_)
    {
        return;
    }

    for (auto recovery_request = recovery_client_->getNextRequest(); recovery_request.has_value();
         recovery_request = recovery_client_->getNextRequest())
    {
        auto pg = getProcessGroup(recovery_request->process_group_identifier_);

        if (nullptr == pg)
        {
            LM_LOG_ERROR() << "recoveryActionHandler: Unknown process group " << recovery_request->process_group_identifier_;
            continue;
        }

        const IdentifierHash old_state = pg->getProcessGroupState();
        const IdentifierHash recovery_state =
                configuration_manager_.getNameOfRecoveryState(pg->getProcessGroupName());
        const GraphState graph_state = pg->getState();

        LM_LOG_DEBUG() << "recoveryActionHandler: Processing recovery request for PG "
                       << recovery_request->process_group_identifier_ << " to state " << recovery_state;

        auto& tracker = trackers_[pg->getProcessGroupIndex()];

        if (GraphState::kInTransition == graph_state)
        {
            if (old_state != recovery_state)
            {
                // Cancel current transition and queue recovery
                tracker.pending = TransitionContext{recovery_state, std::nullopt};
                pg->setRequestStartTime();
                pg->cancel();
            }
            else
            {
                // Already in transition to the requested state
                LM_LOG_DEBUG() << "recoveryActionHandler: Already transitioning to same state";
            }
        }
        else if (GraphState::kSuccess == graph_state && old_state == recovery_state)
        {
            // Already in the requested state
            LM_LOG_DEBUG() << "recoveryActionHandler: Already in requested state";
        }
        else
        {
            // Start new state transition
            tracker.pending = TransitionContext{recovery_state, std::nullopt};
            pg->setRequestStartTime();
        }
    }
}

inline void ProcessGroupManager::processGroupHandler(Graph& pg, TransitionTracker& tracker)
{
    // check to see if there is a state change request to process
    // If current pg not in transition and there is a pending request state
    // start the transition, produce immediate response if that fails.
    GraphState graph_state = pg.getState();

    if (GraphState::kSuccess == graph_state || GraphState::kUndefinedState == graph_state)
    {
        if (tracker.pending)
        {
            IdentifierHash target = tracker.pending->target_state;

            if ((target != pg.getProcessGroupState()) || (GraphState::kUndefinedState == graph_state))
            {
                ProcessGroupStateID pgs;
                pgs.pg_name_ = pg.getProcessGroupName();
                pgs.pg_state_name_ = target;

                // Move pending to in_flight before starting transition
                tracker.in_flight = std::move(tracker.pending);
                tracker.pending.reset();

                LM_LOG_DEBUG() << "Start transition to" << pgs.pg_state_name_ << "for PG" << pgs.pg_name_;

                if (!pg.startTransition(pgs))
                {
                    pg.setPendingEvent(ControlClientCode::kSetStateInvalidArguments);
                }
            }
            else
            {
                // Target == current state, discard pending
                tracker.pending.reset();
            }
        }

        if (GraphState::kUndefinedState == pg.getState() && !tracker.pending && !tracker.in_flight)
        {
            // Graph is in an error state (kUndefinedState) and there is no valid request
            // from outside to change this situation — perform fallback recovery

            ProcessGroupStateID recovery_state;
            recovery_state.pg_name_ = pg.getProcessGroupName();
            recovery_state.pg_state_name_ = configuration_manager_.getNameOfRecoveryState(recovery_state.pg_name_);

            LM_LOG_WARN() << "Problem discovered in PG" << recovery_state.pg_name_ << "Activating Recovery state.";

            tracker.in_flight = TransitionContext{recovery_state.pg_state_name_, std::nullopt};
            pg.setRequestStartTime();
            pg.startTransition(recovery_state);
        }
    }
}

void ProcessGroupManager::setInitialStateTransitionResult(ControlClientCode result)
{
    initial_state_transition_result_ = result;
    nudgeMainLoop();
}

void ProcessGroupManager::nudgeMainLoop()
{
    main_loop_sem_.post();
}

score::concurrency::InterruptibleFuture<void> ProcessGroupManager::ActivateRunTarget(IdentifierHash target_id)
{
    score::concurrency::InterruptiblePromise<void> promise;
    auto future = promise.GetInterruptibleFuture().value();

    if (!activation_queue_.tryEnqueue(ActivationRequest{target_id, std::move(promise)}))
    {
        LM_LOG_ERROR() << "ActivateRunTarget: activation queue full, dropping request";
    }
    else
    {
        nudgeMainLoop();
    }

    return future;
}


std::shared_ptr<ProcessInfoNode> ProcessGroupManager::getProcessInfoNode(uint32_t pg_index, uint32_t process_index)
{
    std::shared_ptr<ProcessInfoNode> result = nullptr;

    if (pg_index < process_groups_.size())
    {
        result = process_groups_[pg_index]->getProcessInfoNode(process_index);
    }

    return result;
}

std::shared_ptr<Graph> ProcessGroupManager::getProcessGroup(IdentifierHash pg_name)
{
    /* we could use a map, we could use std::find_if
       however, there's not many process groups so gain of using a map
       is small, and it seems simpler just to write a simple loop.
     */
    std::shared_ptr<Graph> result = nullptr;

    for (auto pg : process_groups_)
    {
        if (pg->getProcessGroupName() == pg_name)
        {
            result = pg;
            break;
        }
    }

    return result;
}

osal::IProcess* ProcessGroupManager::getProcessInterface()
{
    return &process_interface_;
}

ConfigurationManager* ProcessGroupManager::getConfigurationManager()
{
    return &configuration_manager_;
}

std::shared_ptr<SafeProcessMap> ProcessGroupManager::getProcessMap()
{
    return process_map_;
}

std::shared_ptr<JobQueue<ProcessInfoNode>> ProcessGroupManager::getWorkerJobs()
{
    return worker_jobs_;
}

}  // namespace internal

}  // namespace lcm

}  // namespace score
