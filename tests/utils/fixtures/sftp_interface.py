from .file_interface import FileInterface
from itf.core.com.sftp import Sftp
from pathlib import Path
import pytest
import logging

logger = logging.getLogger(__name__)
class SftpInterface(FileInterface):

    def __init__(self, sftp):
        self.__sftp = sftp
        pass

    def download(self, remote_path: Path, local_path: Path):
        return self.__sftp.download(remote_path, local_path)

    def upload(self, local_path: Path, remote_path: Path):
        logger.info(f"Upload: {local_path} to {remote_path}")
        return self.__sftp.upload(local_path, remote_path)

    def walk(self, remote_path):
        for p,f in self.__sftp.walk(remote_path):
            yield p,f


@pytest.fixture
def file_interface(control_interface):
    logger.info("Starting SFTP Connection")
    with Sftp(control_interface.ssh, "not_needed") as sftp:
        yield SftpInterface(sftp)
        logger.info("Closing SFTP Connection")
