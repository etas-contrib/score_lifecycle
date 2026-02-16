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

from tests.utils.fixtures.target import target
# from tests.utils.fixtures.ssh import ssh
from tests.utils.fixtures import control_interface
from tests.utils.fixtures.sftp_interface import file_interface

from time import sleep
import logging
logger = logging.getLogger(__name__)

def test_one(control_interface, file_interface):
    ret, stdout, stderr = control_interface.exec_command_blocking("ls /opt/score/launch_manager")
    assert ret == 0, f"lcm is not installed! {stderr}"

    ret, stdout, stderr = control_interface.exec_command_blocking("/opt/score/launch_manager")
    logger.error(stdout)
    logger.error(stderr)

    file_interface.upload("/home/kam1yok/README.md", "/tmp/read")
    ret, stdout, stderr = control_interface.exec_command_blocking("ls /tmp")
    logger.error(stdout)
    logger.error(stderr)

    
