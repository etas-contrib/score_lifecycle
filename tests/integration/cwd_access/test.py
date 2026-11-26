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
from tests.utils.testing_utils.setup_test import setup_test
from tests.utils.testing_utils.run_test import run_test
from tests.utils.testing_utils.test_results import assert_test_results
from attribute_plugin import add_test_properties


@add_test_properties(
    partially_verifies=[
        "comp_req__launch_man__cwd_support",
        "feat_req__lifecycle__sandbox_options",
    ],
    test_type="requirements-based",
    derivation_technique="explorative-testing",
)
def test_cwd_access(target, setup_test, assert_test_results, remote_test_dir):
    """
    Objective: Verifies that the configured working directory does not give a process access to files it must not access.

    The launch manager starts the state manager (control_client_test_driver) as root. It creates /tmp/locked
    (mode 0700, root only) containing visible/secret.txt (accessible to everyone) and then activates the Run Target
    "cwd_access". It starts cwd_access_process as uid/gid 1000 with the working directory /tmp/locked/visible and the
    ready condition "Terminated". The process opens secret.txt by its absolute path and relative to its working
    directory.
    Expected Behaviour: Both opens fail with EACCES, so the process exits with 0 and the activation of "cwd_access"
    succeeds.
    """

    run_test(
        target=target,
        binary_path=str(remote_test_dir / "launch_manager"),
        args=["-c", str(remote_test_dir / "etc/config.bin")],
        cwd=str(remote_test_dir),
    )

    assert_test_results({"control_client_test_driver.xml"})
