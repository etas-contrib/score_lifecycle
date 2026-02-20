from tests.utils.fixtures.target.fixture import target
from tests.utils.fixtures.control_interface.control_interface import ControlInterface
from itf.core.com.ssh import execute_command, Ssh
from typing import Tuple
from pathlib import Path
import pytest
import logging
import time


logger = logging.getLogger(__name__)

class SshInterface(ControlInterface):
    """
    """

    def __init__(self, ssh: Ssh):
        self.__ssh = ssh

    def exec_command_blocking(self,
        *args: str, timeout=1, **env: str
    ) -> Tuple[int, str, str]:
        cmd = ' '.join(args)
        logger.debug(f"Running command \'{cmd}\'")
        env = {
            "PATH": "/proc/boot",
        }
        stdin, stdout, stderr = self.__ssh.exec_command(cmd, environment =env)

        ret_code = stdout.channel.recv_exit_status()

        return ret_code, stdout.read().decode('utf-8'), stderr.read().decode('utf-8')


    def run_until_file_deployed(self,
        *args,
        timeout=1,
        file_path=Path("/opt/score/tests/smoke/test_end"),
        poll_interval=0.05,
        **env,
    ) -> Tuple[int, str, str]:

        cmd = ' '.join(str(arg) for arg in args)
        file_path_str = str(file_path)
        environment = env if env else None
        
        stdin, stdout, stderr = self.__ssh.exec_command(cmd, environment=environment)
        channel = stdout.channel
        
        start_time = time.time()
        file_found = False
        process_ended = False
        
        try:
            while True:
                # check if timeout exceeded
                if (time.time() - start_time) > timeout:
                    # timeout - kill the process
                    stdout.channel.close()
                    return (-1, 
                            stdout.read().decode('utf-8', errors='replace'),
                            stderr.read().decode('utf-8', errors='replace'))
                
                # check if process has ended
                if channel.exit_status_ready():
                    process_ended = True
                    break
                
                # check if file exists on remote system
                check_cmd = f"test -f {file_path_str} && echo 'EXISTS' || echo 'NOT_EXISTS'"
                _, check_stdout, _ = self.__ssh.exec_command(check_cmd)
                result = check_stdout.read().decode('utf-8', errors='replace').strip()
                
                if result == 'EXISTS':
                    file_found = True
                    break
                else:
                    logger.debug("file not found")
                
                logger.debug("waiting")
                time.sleep(poll_interval)
            
            # if file was found, kill the process
            if file_found:
                stdout.channel.close()
                return (-1,
                        stdout.read().decode('utf-8', errors='replace'),
                        stderr.read().decode('utf-8', errors='replace'))
            
            # process ended naturally
            exit_code = channel.recv_exit_status()
            output = stdout.read().decode('utf-8', errors='replace')
            error = stderr.read().decode('utf-8', errors='replace')
            
            return (exit_code, output, error)
            
        except Exception as e:
            # ensure we close the channel on any exception
            try:
                stdout.channel.close()
            except:
                pass
            
            try:
                output = stdout.read().decode('utf-8', errors='replace')
                error = stderr.read().decode('utf-8', errors='replace')
            except:
                output = ""
                error = str(e)
            
            return (-1, output, error)

        

    @property
    def ssh(self):
        return self.__ssh

