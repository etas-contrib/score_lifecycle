from .file_interface import FileInterface
from itf.core.com.sftp import Sftp
from pathlib import Path
import pytest


class SftpInterface(FileInterface):

    def __init__(self, sftp):
        self.__sftp = sftp
        pass

    def download(self, remote_path: Path, local_path: Path):
        self.__sftp.download(remote_path, local_path)

    def upload(self, local_path: Path, remote_path: Path):
        self.__sftp.upload(local_path, remote_path)


@pytest.fixture
def file_interface(control_interface):
    with Sftp(control_interface.ssh, "not_needed") as sftp:
        yield SftpInterface(sftp)
