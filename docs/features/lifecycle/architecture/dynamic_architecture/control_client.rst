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

##############
Control Client
##############

:need:`comp__lifecycle_control_client`

The following use cases are supported by the `ControlInterface` provided by the
:term:`Launch Manager`.

**Activating a Run Target**

When a request to activate a run target is received via the `ControlInterface`,
the :term:`Launch Manager` shall perform the following operations:

1. **Validation**: Evaluate if the conditions are correct for activating the
   requested run target:
   - The run target exists in the configuration
   - All dependencies for the run target are resolvable
   - Required resources are available

2. **Transition Logic**: Determine the transition from the current state to the
   target state:
   - If a different run target is active, perform a switch operation (stop current, start requested)
   - If the same run target is already active, verify its state and potentially restart failed components

3. **Execution**: Execute the transition in the correct dependency order: -
   Stop components that are not part of the new run target - Start components
   that are required for the new run target - Respect dependency relationships
   during both stop and start operations

4. **Response**: Return status to the caller: - Success if all components
   transitioned correctly - Failure with detailed error information if any
   component failed to transition

This unified approach allows external state managers to request any run target
activation without needing to know the current system state, as the
:term:`Launch Manager` handles the transition logic internally.

.. feat_arc_dyn:: Control Client to Launch Manager Interaction
    :id: feat_arc_dyn__lifecycle__dv_cc_lcm
    :security: YES
    :safety: ASIL_B
    :version: 1
    :status: invalid
    :fulfils: feat_req__lifecycle__control_commands[version==1],
              feat_req__lifecycle__launch_support[version==1],
              feat_req__lifecycle__request_run_target_start[version==1],
              feat_req__lifecycle__run_target_support[version==1],
              feat_req__lifecycle__start_named_run_target[version==1],
              feat_req__lifecycle__switch_run_targets[version==1],
    :belongs_to: feat__lifecycle

    .. uml:: _assets/control_interface_start_sequence.puml
       :scale: 50
       :align: center
