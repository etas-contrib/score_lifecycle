..
   # *******************************************************************************
   # Copyright (c) 2025 Contributors to the Eclipse Foundation
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

Launch manager
##############

The :term:`Launch Manager` is a component that provides a framework for managing
the lifecycle of processes in the S-CORE platform.
It allows for launching, monitoring, and controlling processes based on defined
configurations and requirements.
As such, it is a central part of the lifecycle management in S-CORE and knows
about the state of all processes in the system.

It's foreseen ECU projects will need a custom state management to fulfill
ECU-project specific requirements.
The S-CORE stack will offer a framework to control application lifecycle, but
will not specify the State Manager.

Overview
========

The functionality of the :term:`Launch Manager` is defined by configuration
data, which spawns a directed acyclic graph (DAG) of :term:`Components
<Component>` and so called :term:`Run Targets <Run Target>`.
The :term:`Run Targets <Run Target>` are virtual nodes in the DAG and represent
:term:`Run States <Run State>` of the system.
The :term:`Launch Manager` is responsible for starting and stopping the
processes in the correct order, based on the dependencies defined in the
configuration data.

E.g. the configuration below consists of three :term:`Run Targets <Run Target>`
managing 9 components. If the user selects e.g. the :term:`Run Target` "debug"
the :term:`Launch Manager` will start the components in the following order
defined by the dependencies.

1. flash driver
2. filesystem
3. setup filesystems
4. networking
5. ssh

.. uml:: _assets/launch_manager_target_tree.puml
   :scale: 50
   :align: center

The :need:`comp__lifecycle_launch_manager` implements the following interfaces,for the selection of :term:`Run Target` s, starting and stopping of components and monitoring of the processes.

Switching between Run Targets
-----------------------------

The :term:`Launch Manager` allows switching between different :term:`Run Targets <Run Target>`. When a switch is requested, the :term:`Launch Manager` evaluates the current state and the target state,
determining which components need to be started or stopped based on their dependencies.

When a component is started the :term:`Launch Manager` will start the corresponding process and monitor its state via :term:`Ready Conditions <Ready Condition>`.

:term:`Ready Conditions <Ready Condition>` are essential mechanisms that determine when a component has successfully completed its startup phase and is ready to fulfill its intended role in the system. These conditions provide flexibility in defining what constitutes a "ready" state for different types of components. For SCORE applications, components can actively report their readiness through the Lifecycle Interface by signaling specific states or custom conditions. For native applications, the :term:`Launch Manager` relies on external indicators such as process existence, file creation, network socket availability, or successful process termination. This dual approach ensures that both modern SCORE-aware applications and legacy native applications can participate in the dependency management system, allowing the :term:`Launch Manager` to orchestrate complex startup sequences where components depend on each other's readiness rather than just their launch order.



Requirements
------------

- :need:`feat_req__lifecycle__control_commands`
- :need:`feat_req__lifecycle__request_run_target_start`
- :need:`feat_req__lifecycle__switch_run_targets`

