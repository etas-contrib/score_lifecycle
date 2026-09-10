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

Lifecycle Client
################

The following use cases are supported by the Lifecycle Interface, with
different capabilities depending on the application type.

**SCORE Application State Communication**

SCORE applications implementing the Lifecycle Interface can communicate their
internal state to the :term:`Launch Manager`. The state information includes:

- **Started**: Application has successfully initialized and is ready to operate
- **Running**: Application is actively executing its main functionality
- **Stopped**: Application has terminated or is in the process of shutting down

The :term:`Launch Manager` uses this state information for:

- Dependency resolution for other applications
- Recovery action decisions
- Status reporting to external state managers via the Control Interface


**SCORE Application Conditional Signaling**

SCORE applications can signal custom conditions to the :term:`Launch Manager` via the Alive Interface. This enables:

- Complex dependency management beyond simple process startup
- Coordination between interdependent applications

Custom conditions can be used by other applications as launch dependencies, allowing for sophisticated startup orchestration.

**Native Application Control**

Native applications that do not implement the Lifecycle Interface are controlled through POSIX signals:

- **SIGTERM**: Graceful shutdown request
- **SIGKILL**: Forced termination (after timeout)
- **SIGUSR1/SIGUSR2**: Application-specific signals (if configured)

The :term:`Launch Manager` monitors native applications through:

- Process ID (PID) tracking
- Exit code evaluation
- Resource usage monitoring via OS facilities
- Timeout-based failure detection

For native applications, the :term:`Launch Manager` provides:

- Basic lifecycle control (start/stop)
- Simple dependency management based on process existence
- Configurable startup/shutdown timeouts
- Exit code-based success/failure determination

.. comp:: Lifecycle Client
   :id: comp__lifecycle_lifecycle_client
   :status: valid
   :version: 1
   :safety: ASIL_B
   :implements: logic_arc_int__lifecycle__lifecycle_if[version==1],
   :uses: logic_arc_int__log_cpp__logging[version==1],
          logic_arc_int__os__unistd[version==1],
   :security: NO
   :belongs_to: feat__lifecycle[version==1]

   TODO


.. toctree::

   lifecycle.md
   lifecyclemanager.md
