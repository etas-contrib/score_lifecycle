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


Control Client Architecture
===========================

This interface provides control functionality for activating and managing run
targets.It allows users to trigger execution of configured :term:`Run targets
<Run target>` through a standardized activation mechanism.

Interface
---------

The control interface is defined here:
:need:`logic_arc_int__lifecycle__controlif`
The :term:`Launch Manager` provides an interface, which allows an external
State Manager application to request the :term:`Launch Manager` to start, stop
or restart applications or groups of applications, which allows the
implementation of a state management applications to support dynamic state
control.

.. comp:: Control Client
   :id: comp__lifecycle_control_client
   :status: valid
   :version: 1
   :safety: ASIL_B
   :implements: logic_arc_int__lifecycle__controlif[version==1],
                logic_arc_int__lifecycle__alive_if[version==1]
   :uses: logic_arc_int__log_cpp__logging[version==1],
          logic_arc_int__os__unistd[version==1],
          logic_arc_int__lifecycle__lifecycle_if[version==1]
   :security: NO
   :belongs_to: feat__lifecycle[version==1]

   TODO
