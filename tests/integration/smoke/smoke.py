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
from tests.utils.fixtures.setup_test import ( setup_tests,
    download_test_results,
    test_dir
)
from tests.utils.fixtures.sftp_interface import file_interface
from tests.utils.fixtures.check_for_failures import check_for_failures
from tests.utils.fixtures import control_interface
from tests.utils.fixtures.target import target

import logging

def test_smoke(setup_tests, control_interface, download_test_results, test_dir):

    code, stdout, stderr = control_interface.run_until_file_deployed(
        "cd /opt/score/tests/smoke/bin/ && ./launch_manager"
    )
    logging.info(stdout)
    logging.info(stderr)

    download_test_results()
    check_for_failures(test_dir, 2)
