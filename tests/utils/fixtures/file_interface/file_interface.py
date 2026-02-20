from abc import ABC, abstractmethod
from pathlib import Path

class FileInterface(ABC):

    def download(remote_path: Path, local_path: Path):
        raise NotImplementedError()

    def upload(remote_path: Path, local_path: Path):
        raise NotImplementedError()
