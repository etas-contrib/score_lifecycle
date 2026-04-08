..
   # *******************************************************************************
   # Copyright (c) 2026 Contributors to the Eclipse Foundation
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


.. _lm_conf_launch_manager_configuration:

Launch Manager Configuration
############################

This document describes the configuration schema for the S-CORE **Launch Manager**, covering core concepts and providing detailed technical insights into how the **Launch Manager** configuration is structured and operates.

.. _lm_conf_introduction_to_launch_manager_configuration:

Introduction to Launch Manager Configuration
********************************************

Before diving into the specifics of the configuration structure, it is crucial to establish a common language and grasp how the **Launch Manager** orchestrates software components. This introduction is designed to equip new users with the basic understanding necessary to navigate the subsequent detailed configuration discussions.

The **Launch Manager** is an essential system component responsible for managing and orchestrating software units installed on a machine. This orchestration is primarily achieved through the use of **Run Targets**. To fully understand this process, we first need to define three key terms: **Components**, **Run Targets**, and the **Ready State**.

.. _lm_conf_components_the_building_blocks_of_your_system:

Components: The Building Blocks of Your System
==============================================

A **Component**, often referred to as a software component, is an independent, deployable software unit. Examples include applications, device drivers, or software containers. Components are typically developed in isolation and can then be deployed into various target systems, such as physical or virtual machines. During the deployment or integration phase, multiple components are often combined to form a complete, functioning system.

A single component might be deployed across several different systems. To facilitate this, the specific deployment configuration (how and where a component runs) is intentionally kept separate from the inherent properties of the component itself. This separation promotes flexibility and reusability.

.. _lm_conf_ready_state_confirmation_of_operational_readiness:

Ready State: Confirmation of Operational Readiness
==================================================

Each component possesses a **Ready State**. This state signifies that the component has not only started successfully, but has also met all its configured ready conditions. The **Ready State** is crucial because it confirms that a component is fully operational and capable of performing its intended functions, allowing other dependent components, or the system as a whole, to confidently rely on its services.

It is vital to understand that a component's lifecycle, and consequently its **Ready State**, is distinct from the lifecycle of the underlying operating system process initiated during the component's startup sequence.

Consider an example: a script designed to mount a file system. When this component starts, the script will execute and complete its task as soon as the mount operation is finished. The component representing this script, however, will only reach its **Ready State** when the files within that file system become genuinely available for use by other processes. In this scenario, despite the script having finished its execution, the component remains in the **Ready State** because the file system is still mounted and accessible to the rest of the system. This illustrates how the **Ready State** reflects a component's functional availability, rather than merely the availability of its process.

.. _lm_conf_run_targets_grouping_of_components:

Run Targets: Grouping of Components
===================================

A **Run Target** defines a specific collection of components. Essentially, it is a named group that lists the components intended to be active together. When a particular **Run Target** is activated, the **Launch Manager** performs the following sequence of operations to manage the components associated with it:

* **Deactivation of Unassigned Components:** All components currently in the **Ready State** that are **not** part of the **Run Target** being activated will be deactivated. This means that for each component not assigned to the activated **Run Target**, the **Launch Manager** will initiate its shutdown sequence, gracefully terminating the component and its associated process.
* **Activation of Assigned Components:** All components not currently in the **Ready State** but **assigned** to the **Run Target** being activated will be activated. This involves the **Launch Manager** starting each component and waiting for it to successfully reach its **Ready State**.
* **Maintenance of Active Assigned Components:** Any components already in the **Ready State** and also assigned to the **Run Target** being activated will remain active and unchanged.

.. _lm_conf_when_the_launch_manager_starts_a_component:

When the Launch Manager Starts a Component
================================================

With the definitions of **Components**, **Run Targets**, and **Ready State** established, let us clarify the conditions under which the **Launch Manager** will initiate a component's startup sequence. Components will be started for two primary reasons:

* A component is directly assigned to a **Run Target** that is currently being activated.
* Another component, which is assigned to a **Run Target** being activated, explicitly depends on that component.

.. _lm_conf_understanding_dependencies_how_components_and_run_targets_relate:

Understanding Dependencies: How Components and Run Targets Relate
=================================================================

A fundamental aspect of **Launch Manager** configuration involves understanding how components are assigned to a **Run Target** and how dependencies are declared.

When a component is said to be assigned to a **Run Target**, it means the **Run Target's** configuration explicitly lists the component's name within its ``depends_on`` parameter.

Similarly, when a component depends on another component, its own ``component_properties.depends_on`` configuration parameter will list the name of the other component it requires.

Additionally, a **Run Target** can declare a dependency on another **Run Target**. In this scenario, the name of the dependent **Run Target** is listed within the ``depends_on`` configuration parameter of the primary **Run Target**. The most effective way to conceptualize this relationship is to imagine that the list of components assigned to the dependent **Run Target** is effectively included in the list of components of the primary **Run Target**.

.. _lm_conf_rules_for_configuring_dependencies:

Rules for Configuring Dependencies
==================================

When configuring dependencies within the **Launch Manager**, the following rules must be observed to ensure correct system behavior:

* A **Component** can depend on another **Component**.
* A **Run Target** can depend on a **Component**.
* A **Run Target** can depend on another **Run Target**.
* A **Component** cannot depend on a **Run Target**.

Having discussed these basic concepts and the fundamental operation of the **Launch Manager**, we are now ready to explore the detailed configuration structure.
