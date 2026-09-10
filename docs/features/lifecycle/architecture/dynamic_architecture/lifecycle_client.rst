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

#############################
Lifecycle Client Architecture
#############################

:need:`comp__lifecycle_lifecycle_client`

.. feat_arc_dyn:: Lifecycle Client to Launch Manager Interaction
    :id: feat_arc_dyn__lifecycle__lc_lcm
    :security: YES
    :safety: ASIL_B
    :version: 1
    :status: invalid
    :fulfils: feat_req__lifecycle__conditional_startup[version==1],
              feat_req__lifecycle__custom_cond_support[version==1],
              feat_req__lifecycle__component_group_config[version==1],
              feat_req__lifecycle__launch_support[version==1],
              feat_req__lifecycle__monitor_abnormal_term[version==1],
              feat_req__lifecycle__multi_instance_support[version==1],
              feat_req__lifecycle__parallel_launch_support[version==1],
              feat_req__lifecycle__process_ordering[version==1],
              feat_req__lifecycle__process_termination[version==1],
              feat_req__lifecycle__prog_lang[version==1],
              feat_req__lifecycle__liveliness_detection[version==1],
              feat_req__lifecycle__terminationn_dependency[version==1],
    :belongs_to: feat__lifecycle

    .. uml:: _assets/lifecycle_state_machine.puml
       :scale: 50
       :align: center


    .. uml:: _assets/dyn_arch_lcm_lc.puml
       :scale: 50
       :align: center

    .. list-table:: Sequence Diagram Description
       :widths: 10 90
       :header-rows: 1

       * - Sequence number
         - Description
       * - 001
         - Due to a :term:`Run Target` switch, :term:`Launch Manager` activates a process called "Reporting App".


