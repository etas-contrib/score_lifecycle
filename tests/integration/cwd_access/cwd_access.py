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
from tests.utils.testing_utils.setup_test import setup_test  # noqa: F401
from tests.utils.testing_utils.run_test import run_test
from attribute_plugin import add_test_properties


@add_test_properties(
    partially_verifies=[
        "comp_req__launch_man__cwd_support",
        "feat_req__lifecycle__sandbox_options",
    ],
    test_type="requirements-based",
    derivation_technique="requirements-analysis",
)
def test_cwd_access(target, setup_test, remote_test_dir):  # noqa: F811
    """
    Objective: Verifies that the configured working directory does not give a process access to files it must not access.

    A dummy launch manager running as root creates /tmp/locked (mode 0700, root only) containing
    visible/secret.txt (accessible to everyone). It starts a process via osal::startProcess() as uid/gid 1000
    with the working directory /tmp/locked/visible. The process opens secret.txt by its absolute path and
    relative to its working directory.
    Expected Behaviour: Both opens fail with EACCES, so the process and the dummy launch manager exit with 0.
    """

    run_test(
        target=target,
        binary_path=str(remote_test_dir / "dummy_launch_manager"),
        args=[str(remote_test_dir / "cwd_access_process")],
        cwd=str(remote_test_dir),
    )
