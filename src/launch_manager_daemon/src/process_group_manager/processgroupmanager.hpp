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


#ifndef PROCESSGROUPMANAGER_HPP_INCLUDED
#define PROCESSGROUPMANAGER_HPP_INCLUDED

#include <cstdint>
#include <memory>
#include <optional>
#include <ctime>

#include <score/lcm/identifier_hash.hpp>
#include <score/lcm/internal/control_client_codes.hpp>
#include <configuration_manager/configurationmanager.hpp>
#include <process_group_manager/iprocess.hpp>
#include <process_group_manager/graph.hpp>
#include <process_group_manager/fixed_queue.hpp>
#include <process_group_manager/IGraphControl.hpp>
#include <process_group_manager/jobqueue.hpp>
#include <process_group_manager/oshandler.hpp>
#include <score/lcm/iprocessstatenotifier.hpp>
#include <process_group_manager/processinfonode.hpp>
#include <process_group_manager/safeprocessmap.hpp>
#include <process_group_manager/workerthread.hpp>
#include <process_group_manager/ihealth_monitor_thread.hpp>
#include <score/lcm/internal/osal/semaphore.hpp>
#include <score/lcm/irecovery_client.h>
#include "score/concurrency/future/interruptible_promise.h"

namespace score {

namespace lcm {

namespace internal {

/// @brief Context for a pending or in-flight state transition request
struct TransitionContext {
    IdentifierHash target_state;
    std::optional<score::concurrency::InterruptiblePromise<void>> promise;  // nullopt = no response expected (recovery)
};

/// @brief Activation request queued by ActivateRunTarget() for processing on the main loop
struct ActivationRequest {
    IdentifierHash target_state;
    score::concurrency::InterruptiblePromise<void> promise;
};

static constexpr size_t kMaxPendingActivations = 32;

/// @brief Per-process-group tracker separating in-flight and pending requests
struct TransitionTracker {
    std::optional<TransitionContext> in_flight;  // Currently executing transition
    std::optional<TransitionContext> pending;     // Queued for after current completes
};

/// @brief ProcessGroupManager provides the core functionality of LCM.
/// Software that is deployed to the machine, should be managed through Process Groups.
/// A Process Group (PG) can be described as a set of applications, or executable files, that should be controlled in a coherent way.
/// Through a Process Group, Launch Manager will control the life cycle of Operating System (OS) processes.
/// They will be started and stopped when State Management (SM) request so and they will be started and stopped in a way, that is described by integrator through configuration.
/// When SM request PG change, ProcessGroupManager will use ConfigurationManager to figure out what processes shall be started, or stopped, as well as their startup configuration.
/// Then ProcessGroupManager will use Operating System Abstraction Layer (OSAL) to start, or stop, processes as per configuration.
/// Some of the responsibilities of ProcessGroupManager include:
///     Interaction with ConfigurationManager to ensure that, the list of processes that are running on Machine, is as configured by integrator.
///     Interaction with OSAL to start and stop processes.
///     Interaction with OSAL to discover when processes terminated in an unexpected way.
///     Fulfilling PG State transitions requests from SM, as well as informing SM about unexpected problems (for example process crashes).
class ProcessGroupManager final : public IGraphControl {
   public:
    /// @brief Constructs a new ProcessGroupManager object.
    ///
    /// This constructor initializes the ProcessGroupManager instance,
    /// setting up any necessary internal state and preparing it for use.
    /// @param health_monitor A unique pointer to an IHealthMonitor instance for managing health monitoring.
    /// @param recovery_client A shared pointer to an IRecoveryClient instance for handling recovery operations.
    /// @param process_state_notifier A unique pointer to an IProcessStateNotifier instance for notifying the HM thread of process state changes.
    ProcessGroupManager(std::unique_ptr<IHealthMonitorThread> health_monitor, std::shared_ptr<IRecoveryClient> recovery_client, std::unique_ptr<score::lcm::IProcessStateNotifier> process_state_notifier);

    ~ProcessGroupManager() override;

    /// @brief Initializes the process group manager.
    /// Loads the flat configuration through ConfigurationManager.
    /// Sets up a signal handler for SIGINT and SIGTERM so that the main loop of
    /// the run() method will be exited in the event of those signals
    /// Creates the process map, worker threads and worker job queues.
    /// Creates and initialises the shared memory for the nudge semaphore, always using FD #4,
    /// and stores a pointer to it.
    /// @return Returns true if initialization was successful, false otherwise.
    bool initialize();

    /// @brief De-initialises the process group manager
    /// deletes worker threads, worker jobs and the process map and then de-initialises the configuration manager
    /// un-maps the memory for the nudge semaphore
    void deinitialize();

    /// @brief Self-initiates the state transition to MainPG::Startup (Machine State Startup), then enters
    /// and remains in a loop polling state managers and process groups using the `controlClientHandler()`
    /// and `processGroupHandler()` methods until SIGINT or SIGTERM is received, then transitions all the
    /// process groups to the "Off" state before returning. Each time a piece of work is serviced, wait on
    /// the semaphore so as not to consume cpu cycles unduly.
    /// @return Returns true if the process group manager ran successfully, false otherwise.
    bool run();

    score::concurrency::InterruptibleFuture<void> ActivateRunTarget(IdentifierHash target_id) override;

    /// @brief Get the process group for a given pg_name
    /// @param pg_name the name to look up
    /// @return a pointer to the Graph, or nullptr if not found
    std::shared_ptr<Graph> getProcessGroup(IdentifierHash pg_name);

    /// @brief Get a node corresponding to the given process group and process index
    /// @param pg_index The index of the process group in the list of groups managed by this manager
    /// @param process_index The index of the process in the list of processes in the process group
    /// @return nullptr if the node does not exist, otherwise a pointer to the corresponding node.
    std::shared_ptr<ProcessInfoNode> getProcessInfoNode(uint32_t pg_index, uint32_t process_index);

    /// @brief set the initial machine group state change result, called by graph when the transition completes
    /// @param result the result to save; it can only be saved once
    void setInitialStateTransitionResult(ControlClientCode result);

    /// @brief Wake up the main loop (called by Graph when a pending event is set)
    void nudgeMainLoop();

    /// @brief Gets the process interface.
    /// @return Pointer to the OSAL process interface.
    osal::IProcess* getProcessInterface();

    /// @brief Gets the configuration manager.
    /// @return Pointer to the ConfigurationManager object.
    ConfigurationManager* getConfigurationManager();

    /// @brief Gets the process map.
    /// @return Shared pointer to the SafeProcessMap object.
    std::shared_ptr<SafeProcessMap> getProcessMap();

    /// @brief Gets the job queue for worker threads.
    /// @return Shared pointer to the JobQueue object for ProcessInfoNode jobs.
    std::shared_ptr<JobQueue<ProcessInfoNode>> getWorkerJobs();

    /// @brief Calls QueuePosixProcess method of psn data member
    /// @details Writes via IPC the latest Process State change, so that PHM can be informed about it.
    ///          the PosixProcess structure should be complete at his moment. That means:
    ///          ProcessGroupStateId, ProcessModelled Id, current ProcessState, timestamp are known and set.
    ///          if no more free shared memory, the PosixProcess is not sent.
    /// @param[in]   f_posixProcess   The PosixProcess to be queued
    /// @returns True on success, false for failure (corresponding to kCommunicationError).
    bool queuePosixProcess(const score::lcm::PosixProcess& f_posixProcess) {
        return process_state_notifier_->queuePosixProcess(f_posixProcess);
    }

    /// @brief Cancels processGroupManager main routine as though SIGTERM had been sent
    void cancel();

    /// @brief Set the internal pointer for the Launch Manager ProcessInfoNode
    void setLaunchManagerConfiguration(const OsProcess* launch_manager_config);


   private:
    /// @brief Process queued activation requests from the FixedQueue (called from main loop)
    void processTransitionRequests();

    /// @brief Resolve promises for completed transitions
    void processTransitionResponses();

    /// @brief Handle recovery actions requested by the Health Monitor
    void recoveryActionHandler();

    /// @brief Manage the process group by starting any pending transitions that were requested
    /// @details If the Graph is in the correct state to start a transition (i.e. `kSuccess` or `kUndefined`)
    /// and the pending state is valid and not equal to the last requested state, attempt to start
    /// the transition. If starting the transition fails, it must be because the requested state
    /// is invalid, so set the pending response to `kSetStateInvalidArguments`.
    /// @param pg Reference of the process group to manage.
    /// @param tracker Reference to the transition tracker for this process group.
    void processGroupHandler(Graph& pg, TransitionTracker& tracker);

    /// @brief Start the initial transition to the machine process group startup state.
    /// @details initial machine process group state pointer is retrieved from configuration manager and if
    /// the value of not null and a graph for a process group with the correct name exists the
    /// transistion of that process group to the required state is started by calling the graph's
    /// `startInitialTransition()` method.
    /// @return true if the initial transition was started, false otherwise
    bool startInitialTransition();


    /// @brief Send all process groups to the "Off" state
    /// @details cancel any Graph for a process group not in the "Off" state, wait for up to 2 seconds for all graphs
    /// to be no longer in the `kCancelled` state, start a transition of remaining process groups to "Off" state,
    /// and finally wait for up to a second for all graphs to complete.
    /// @warning Side effect: Depending if it is needed to forcefully terminate processes, worker jobs might be stopped after this call
    void allProcessGroupsOff();

    /// @brief Initializes the process groups.
    /// @return Returns true if initialization was successful, false otherwise.
    inline bool initializeProcessGroups();

    /// @brief Creates process component objects, including the job queue and worker threads.
    inline void createProcessComponentsObjects();

    /// @brief Initializes the graph nodes.
    inline void initializeGraphNodes();


    /// @brief The ConfigurationManager object associated with the ProcessGroupManager.
    ConfigurationManager configuration_manager_;

    /// @brief The process interface object associated with the ProcessGroupManager.
    osal::IProcess process_interface_;

    /// @brief Shared pointer to the SafeProcessMap object.
    std::shared_ptr<SafeProcessMap> process_map_;

    /// @brief Unique pointer to the worker threads handling ProcessInfoNode jobs.
    std::unique_ptr<WorkerThread<ProcessInfoNode>> worker_threads_;

    /// @brief Shared pointer to the job queue for ProcessInfoNode jobs.
    std::shared_ptr<JobQueue<ProcessInfoNode>> worker_jobs_;

    /// @brief Total number of processes.
    /// @deprecated there is no reason to store the total number of processes in the class
    /// @todo Remove this data member, use a local variable and pass it as a parameter to
    /// the functions that require it
    uint32_t total_processes_ = 0U;

    /// @brief Number of process groups.
    /// @deprecated there is no reason to store the number of process groups in the class
    /// @todo Remove this data member, it is not required (a local variable may be used)
    uint32_t num_process_groups_ = 0U;

    /// @brief Stores the process groups as shared pointers to Graph objects.
    std::vector<std::shared_ptr<Graph>> process_groups_{};

    /// @brief Per-process-group transition trackers (parallel to process_groups_)
    std::vector<TransitionTracker> trackers_{};

    /// @brief The result of the initial state transition
    std::atomic<ControlClientCode> initial_state_transition_result_{ControlClientCode::kInitialMachineStateNotSet};

    /// @brief Pointer to the graph corresponding to the machine process group
    std::shared_ptr<Graph> machine_process_group_{nullptr};

    /// @brief Thread-safe queue for activation requests from ActivateRunTarget()
    FixedQueue<ActivationRequest, kMaxPendingActivations> activation_queue_;

    /// @brief Process state notifier object used to send data to PHM
    std::unique_ptr<score::lcm::IProcessStateNotifier> process_state_notifier_;

    /// @brief pointer to the configuration for Launch Manager
    const OsProcess* launch_manager_config_{nullptr};

    std::unique_ptr<IHealthMonitorThread> health_monitor_thread_;

    std::shared_ptr<score::lcm::IRecoveryClient> recovery_client_{};

    /// @brief Semaphore used to wake up the main loop from any event source
    osal::Semaphore main_loop_sem_;
};

}  // namespace lcm

}  // namespace internal

}  // namespace score

#endif  /// PROCESSGROUPMANAGER_HPP_INCLUDED
