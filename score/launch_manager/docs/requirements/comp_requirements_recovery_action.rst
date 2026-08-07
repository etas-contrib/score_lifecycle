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

Recovery Actions
================

The following requirements detail the behaviour of :term:`Recovery Action`\ s
performed by the :term:`Launch Manager` when a :term:`Component` fails. A
:term:`Component` fails when the supervision of the :term:`Component` fails, or
when its :term:`Process` exits with a non-zero return code or crashes. A failure
before the :term:`Ready Condition` is fulfilled is a *startup* failure, a failure
afterwards is a *runtime* failure.

A :term:`Recovery Action` has one of the following actions:

* *Restart component* - the failed :term:`Component` is restarted.
* *Switch run target* - the :term:`Launch Manager` transitions to another
  :term:`Run Target`.

Three flavours of :term:`Recovery Action` exist:

* *Ready Recovery Action* - triggered by a startup failure of a :term:`Component`,
  limited to the *restart component* action.
* *Ordinary Recovery Action* - triggered by a runtime failure of a
  :term:`Component` (e.g. a supervision failure).
* *Fallback Recovery Action* - the last resort action that transitions the system
  into a minimal diagnosable state.

Recovery Action Levels and Types
--------------------------------

.. comp_req:: Component level recovery action is optional
    :id: comp_req__launch_man__ra_component_optional
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recovery_action_support[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall support an optional :term:`Recovery Action`
    per :term:`Component`. If no :term:`Recovery Action` is configured for a
    failed :term:`Component`, the :term:`Recovery Action` of the :term:`Run Target`
    shall be triggered instead.

.. comp_req:: Run target level recovery action is mandatory
    :id: comp_req__launch_man__ra_run_target_mandatory
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recov_run_target_switch[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall require a :term:`Recovery Action` to be
    configured for every :term:`Run Target`, and shall reject a configuration in
    which a :term:`Run Target` has no :term:`Recovery Action` defined.

.. comp_req:: Mandatory fallback recovery action
    :id: comp_req__launch_man__ra_fallback_mandatory
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recov_run_target_switch[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall require exactly one *Fallback Recovery
    Action* to be configured, which is used as the last resort when the
    :term:`Recovery Action` of a :term:`Run Target` fails. The *Fallback Recovery
    Action* shall transition the system into a minimal diagnosable state by
    switching to a configured fallback :term:`Run Target`.

.. comp_req:: Component recovery action types
    :id: comp_req__launch_man__ra_component_action_types
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recovery_action_support[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall support, for an *Ordinary Recovery Action*
    defined on :term:`Component` level, either restarting the failed
    :term:`Component` or switching to another :term:`Run Target`.

.. comp_req:: Run target and fallback recovery action type
    :id: comp_req__launch_man__ra_switch_only
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recov_run_target_switch[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall limit the action of a :term:`Recovery Action`
    defined on :term:`Run Target` level and the action of the *Fallback Recovery
    Action* to switching to another :term:`Run Target`.

.. comp_req:: Ready recovery action limited to restart
    :id: comp_req__launch_man__ra_ready_restart_only
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recovery_action_support[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall limit the action of a *Ready Recovery Action*
    to restarting the failed :term:`Component`.

Recovery Action Triggering and Escalation
-----------------------------------------

.. comp_req:: Trigger recovery on component failure
    :id: comp_req__launch_man__ra_trigger_on_failure
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__liveliness_detection[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall trigger a :term:`Recovery Action` when a
    :term:`Component` fails, i.e. when its supervision fails, when its
    :term:`Process` exits with a non-zero return code, or when its
    :term:`Process` crashes.

.. comp_req:: Select ready or ordinary recovery action
    :id: comp_req__launch_man__ra_ready_vs_ordinary
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__liveliness_detection[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall select the :term:`Component` level
    :term:`Recovery Action` based on the :term:`Ready Condition`:

    * If the :term:`Ready Condition` is not yet fulfilled and a *Ready Recovery
      Action* is configured, the *Ready Recovery Action* shall be triggered.
    * If the :term:`Ready Condition` is not yet fulfilled and no *Ready Recovery
      Action* is configured, the *Ordinary Recovery Action* shall be triggered
      instead, if configured.
    * If the :term:`Ready Condition` is fulfilled, the *Ordinary Recovery Action*
      shall be triggered, if configured.

.. comp_req:: Ready and ordinary recovery actions are independent
    :id: comp_req__launch_man__ra_independent_attempts
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recovery_action_support[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall treat the *Ready Recovery Action* and the
    *Ordinary Recovery Action* of a :term:`Component` as independent. An
    *Ordinary Recovery Action* shall not take the attempts of a preceding *Ready
    Recovery Action* into account, i.e. the configured attempts shall not be
    multiplied across the two :term:`Recovery Action`\ s.

.. comp_req:: Bottom-up recovery escalation order
    :id: comp_req__launch_man__ra_escalation_order
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recovery_action_support[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall escalate :term:`Recovery Action`\ s in
    bottom-up order: first the :term:`Component` level :term:`Recovery Action`,
    then the :term:`Run Target` level :term:`Recovery Action`, and finally the
    *Fallback Recovery Action*. If no :term:`Component` level :term:`Recovery
    Action` is defined, or it fails, the :term:`Run Target` level :term:`Recovery
    Action` shall be triggered. If the :term:`Run Target` level :term:`Recovery
    Action` fails, the *Fallback Recovery Action* shall be triggered.

    .. mermaid::

        flowchart LR
            start([Component failed])
            start --> ready{Ready condition satisfied?}
            ready -- no --> readyra{Ready RA defined?}
            ready -- yes --> ordra{Ordinary component RA defined?}
            readyra -- yes --> do_ready[Invoke Ready RA]
            readyra -- no --> ordra
            do_ready --> ready_ok{Ready RA succeeded?}
            ready_ok -- yes --> done([Recovered])
            ready_ok -- no --> rt
            ordra -- yes --> do_ord[Invoke ordinary RA / Local Recovery]
            ordra -- no --> rt[Invoke Run Target RA / Global Recovery]
            do_ord --> ord_ok{Ordinary RA succeeded?}
            ord_ok -- yes --> done
            ord_ok -- no --> rt
            rt --> rt_ok{Run Target RA succeeded?}
            rt_ok -- yes --> done
            rt_ok -- no --> fb[Invoke Fallback RA / Fallback Recovery]
            fb --> fb_ok{Fallback RA succeeded?}
            fb_ok -- yes --> done
            fb_ok -- no --> wd([Watchdog is fired])

.. comp_req:: Fire watchdog when fallback recovery fails
    :id: comp_req__launch_man__ra_fallback_watchdog
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__smart_watchdog_config[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall let the external :term:`Watchdog` fire, by
    ceasing its :term:`Watchdog` notification, if the *Fallback Recovery Action*
    fails.

.. comp_req:: Recovery escalates to the requested run target
    :id: comp_req__launch_man__ra_requested_run_target
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recov_run_target_switch[version==1]
    :status: valid
    :version: 1

    When a :term:`Component` level :term:`Recovery Action` that switches the
    :term:`Run Target` fails, the :term:`Launch Manager` shall escalate to the
    :term:`Recovery Action` of the currently requested :term:`Run Target`, not to
    the :term:`Recovery Action` of the :term:`Run Target` that the failed
    :term:`Recovery Action` attempted to activate.

Recovery State and Conflict Resolution
--------------------------------------

.. comp_req:: Maintain recovery state of the graph
    :id: comp_req__launch_man__ra_recovery_state
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recovery_action_support[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall maintain a recovery state for the directed
    acyclic graph of :term:`Run Target`\ s and :term:`Component`\ s, with the
    states *NotInRecovery*, *InLocalRecovery*, *InGlobalRecovery* and
    *InFallbackRecovery*.

    .. uml::

        @startuml
        [*] --> NotInRecovery
        NotInRecovery --> NotInRecovery : Ready RA (restart component) invoked
        NotInRecovery --> InLocalRecovery : Restart component RA invoked
        NotInRecovery --> InGlobalRecovery : Switch run target RA invoked
        InLocalRecovery --> NotInRecovery : Restart component RA finished
        InLocalRecovery --> InGlobalRecovery : Switch run target RA invoked / restart RA failed
        InGlobalRecovery --> NotInRecovery : Switch run target RA finished
        InGlobalRecovery --> InFallbackRecovery : Switch run target RA failed
        InFallbackRecovery --> InFallbackRecovery : Fallback RA finished
        @enduml

.. comp_req:: Enter local recovery on restart component action
    :id: comp_req__launch_man__ra_enter_local
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recovery_action_support[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall enter *InLocalRecovery* when a :term:`Recovery
    Action` that restarts a failed :term:`Component` is invoked, and shall return
    to *NotInRecovery* once the :term:`Recovery Action` has successfully finished.

.. comp_req:: Enter global recovery on switch run target action
    :id: comp_req__launch_man__ra_enter_global
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recov_run_target_switch[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall enter *InGlobalRecovery* when a
    :term:`Recovery Action` that switches the :term:`Run Target` is invoked, and
    shall return to *NotInRecovery* once the :term:`Recovery Action` has
    successfully finished.

.. comp_req:: Ready recovery actions do not change recovery state
    :id: comp_req__launch_man__ra_ready_no_state_change
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recovery_action_support[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall process a *Ready Recovery Action* without
    changing the recovery state of the graph. A *Ready Recovery Action* shall be
    accepted and processed regardless of whether the graph is in *NotInRecovery*,
    *InLocalRecovery*, *InGlobalRecovery* or *InFallbackRecovery*.

.. comp_req:: Recovery action priorities
    :id: comp_req__launch_man__ra_priorities
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recovery_action_support[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall resolve conflicts between concurrently
    triggered :term:`Recovery Action`\ s using the following ascending priority:
    a :term:`Recovery Action` that restarts a :term:`Component` has the lowest
    priority, a :term:`Recovery Action` that switches the :term:`Run Target` has a
    higher priority, and the *Fallback Recovery Action* has the highest priority.

.. comp_req:: Allow concurrent restart component recovery actions
    :id: comp_req__launch_man__ra_concurrent_restart
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recovery_action_support[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall allow multiple :term:`Recovery Action`\ s
    that restart a failed :term:`Component` to be processed at the same time,
    independently of whether they are *Ready Recovery Action*\ s or *Ordinary
    Recovery Action*\ s.

.. comp_req:: Single switch run target recovery action wins
    :id: comp_req__launch_man__ra_single_switch
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recov_run_target_switch[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall ignore a triggered :term:`Recovery Action`
    that switches the :term:`Run Target` while another :term:`Recovery Action`
    that switches the :term:`Run Target` is already ongoing, i.e. the first
    triggered :term:`Recovery Action` wins.

.. comp_req:: Switch run target preempts restart component
    :id: comp_req__launch_man__ra_switch_preempts_restart
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recov_run_target_switch[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall, when a :term:`Recovery Action` that switches
    the :term:`Run Target` is triggered, cancel any ongoing :term:`Recovery
    Action` that restarts a failed :term:`Component`. A triggered :term:`Recovery
    Action` that restarts a :term:`Component` shall be ignored while a
    :term:`Recovery Action` that switches the :term:`Run Target` is ongoing.

.. comp_req:: Run target recovery preempts component recovery
    :id: comp_req__launch_man__ra_rt_preempts_component
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recov_run_target_switch[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall, when a :term:`Recovery Action` on
    :term:`Run Target` level is triggered, cancel any ongoing :term:`Recovery
    Action` on :term:`Component` level, independently of its type. A triggered
    *Ordinary Recovery Action* on :term:`Component` level shall be ignored while a
    :term:`Recovery Action` on :term:`Run Target` level is ongoing.

.. comp_req:: Single run target level recovery action
    :id: comp_req__launch_man__ra_single_rt_level
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recov_run_target_switch[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall ignore a triggered :term:`Recovery Action` on
    :term:`Run Target` level while another :term:`Recovery Action` on :term:`Run
    Target` level is already ongoing, i.e. the first triggered :term:`Recovery
    Action` wins.

.. comp_req:: Fallback recovery action preempts all others
    :id: comp_req__launch_man__ra_fallback_preempts
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recov_run_target_switch[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall, when the *Fallback Recovery Action* is
    triggered, cancel all other ongoing :term:`Recovery Action`\ s. An ongoing
    *Fallback Recovery Action* shall not be cancellable by any :term:`Recovery
    Action` on :term:`Component` or :term:`Run Target` level, and such
    :term:`Recovery Action`\ s shall be ignored as long as the *Fallback Recovery
    Action* is ongoing. A triggered *Fallback Recovery Action* shall be ignored if
    a *Fallback Recovery Action* is already ongoing, so that only one *Fallback
    Recovery Action* is ever ongoing.

.. comp_req:: Remain in fallback recovery after success
    :id: comp_req__launch_man__ra_stay_fallback
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recov_run_target_switch[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall remain in *InFallbackRecovery* after the
    *Fallback Recovery Action* has successfully finished. Any subsequent
    :term:`Recovery Action` in this state shall let the :term:`Watchdog` fire. A
    request to switch the :term:`Run Target` via the :term:`Control Interface`
    shall be accepted again after the *Fallback Recovery Action* has successfully
    finished.

.. comp_req:: Ignore control interface run target switch during recovery
    :id: comp_req__launch_man__ra_ignore_control_switch
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recov_run_target_switch[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall ignore a request to switch the :term:`Run
    Target` received via the :term:`Control Interface` while the graph is in
    *InGlobalRecovery*, or while the *Fallback Recovery Action* is being processed
    in *InFallbackRecovery*.

.. comp_req:: Ignore recovery action for terminating originator
    :id: comp_req__launch_man__ra_ignore_terminating
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__liveliness_detection[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall ignore a :term:`Recovery Action` if its
    originator :term:`Process` received a shutdown request before the
    :term:`Recovery Action` is invoked, i.e. when the :term:`Process` is in a
    terminating state.

Recovery Action Timeout
-----------------------

.. comp_req:: Default recovery action timeout
    :id: comp_req__launch_man__ra_default_timeout
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recovery_action_support[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall support a configurable default :term:`Recovery
    Action` timeout, which is applied to any :term:`Recovery Action` that does not
    define a specific timeout.

.. comp_req:: Recovery action specific timeout
    :id: comp_req__launch_man__ra_specific_timeout
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recovery_action_support[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall support a configurable :term:`Recovery
    Action` specific timeout that overrides the default :term:`Recovery Action`
    timeout for that :term:`Recovery Action`.

.. comp_req:: Recovery action timeout maps to transition timeouts
    :id: comp_req__launch_man__ra_timeout_mapping
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recovery_action_support[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall derive the :term:`Recovery Action` timeout
    from the standard timeout values: the :term:`Component` start timeout
    (``ready_timeout``) for a :term:`Recovery Action` that restarts a
    :term:`Component`, and the :term:`Run Target` transition timeout
    (``transition_timeout``) for a :term:`Recovery Action` that switches the
    :term:`Run Target`.

.. comp_req:: Treat recovery action timeout as failure
    :id: comp_req__launch_man__ra_timeout_is_failure
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recovery_action_support[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall treat a :term:`Recovery Action` as failed if
    its processing takes longer than the applicable :term:`Recovery Action`
    timeout, and shall escalate according to the bottom-up escalation order.

Recovery Action Configuration
-----------------------------

.. comp_req:: Configurable restart attempts
    :id: comp_req__launch_man__ra_restart_attempts
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recovery_action_support[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall support a configurable number of restart
    attempts for a :term:`Recovery Action` that restarts a failed
    :term:`Component`.

.. comp_req:: Configurable delay before restart
    :id: comp_req__launch_man__ra_restart_delay
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recovery_action_support[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall support a configurable delay that shall
    elapse before a :term:`Component` is restarted by a :term:`Recovery Action`.

.. comp_req:: Configurable switch run target target
    :id: comp_req__launch_man__ra_switch_target_config
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recov_run_target_switch[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall support configuring, per :term:`Recovery
    Action` that switches the :term:`Run Target`, the :term:`Run Target` to be
    activated.

.. comp_req:: Configurable fallback run target
    :id: comp_req__launch_man__ra_fallback_target_config
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__recov_run_target_switch[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall support configuring the fallback :term:`Run
    Target` that is activated by the *Fallback Recovery Action*.

Recovery Action Logging
-----------------------

.. comp_req:: Log recovery action processing
    :id: comp_req__launch_man__ra_logging
    :reqtype: Functional
    :security: NO
    :safety: ASIL_B
    :derived_from: feat_req__lifecycle__logging_support[version==1]
    :status: valid
    :version: 1

    The :term:`Launch Manager` shall log the triggering, the type, and the result
    (success or failure) of every :term:`Recovery Action`, as well as every
    transition of the recovery state of the graph.
