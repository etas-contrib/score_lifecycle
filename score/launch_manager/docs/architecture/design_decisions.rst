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

.. _lifecycle_design_decisions:

Design Decisions
################

Launch Manager uses posix_spawn() to start processes on QNX
===========================================================

.. dec_rec:: posix_spawn on QNX
   :id: dec_rec__lifecycle__posix_spawn
   :status: accepted
   :version: 1
   :context: Architecture
   :decision: The Launch Manager uses posix_spawn() to start processes on QNX and fork() + execve() on Linux
   :consequences: Faster process starts on QNX, separate QNX and Linux implementations behind a common OSAL interface
   :affects: comp__lifecycle_launch_manager

Context
-------

The Launch Manager starts the processes of the system, which may include all processes during the startup of the machine.
The time needed to start a process directly adds to the startup time, so starting processes has to be efficient.

Before a new process executes its first instruction, the Launch Manager has to apply the configured attributes,
e.g. user and group IDs, supplementary groups, scheduling policy and priority, CPU affinity, resource limits,
working directory and process group.

Options Considered
------------------

Option A: Use fork() + execve() on QNX and Linux
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Pros:

* One implementation for both platforms
* fork() is cheap on Linux due to copy-on-write
* All attributes can be applied in the child process between fork() and execve()

Cons:

* fork() is considerably slower than posix_spawn() on QNX
* The code between fork() and execve() must only call async-signal-safe functions, which is difficult to guarantee

Option B: Use posix_spawn() on QNX and Linux
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Pros:

* posix_spawn() is considerably faster than fork() on QNX
* No code runs in the child process between process creation and execution of the new program

Cons:

* The POSIX standard posix_spawn() cannot set most of the required attributes, e.g. user and group IDs,
  supplementary groups, CPU affinity and resource limits.
  QNX closes this gap with platform-specific extensions, Linux does not.
* Linux provides no way to start the process held, so the missing attributes cannot be applied afterwards
  before the new program runs.
  The required functionality therefore cannot be implemented with posix_spawn() on Linux.

Option C: Use posix_spawn() on QNX and fork() + execve() on Linux
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Pros:

* Fast process starts on QNX using posix_spawn() and its QNX-specific extensions
* Fast process starts on Linux using fork() + execve()
* All required attributes can be applied on both platforms

Cons:

* Two implementations to maintain, one for QNX and one for Linux
* On Linux, the code between fork() and execve() must only call async-signal-safe functions,
  which is difficult to guarantee

Evaluation
----------

Option A is ruled out, because the performance penalty of fork() on QNX is too high.

Option B is ruled out, because posix_spawn() on Linux cannot apply the required process attributes.

Option C provides the best performance on both platforms and supports all required attributes.

Decision
--------

Option C: The Launch Manager uses posix_spawn() to start processes on QNX and fork() + execve() on Linux.
We accept the platform-specific implementations in exchange for the best performance on both platforms.

Consequences
------------

Processes are started differently on QNX and Linux.
The difference is hidden behind a common interface in the OSAL layer of the Launch Manager,
so the rest of the Launch Manager is not affected.
