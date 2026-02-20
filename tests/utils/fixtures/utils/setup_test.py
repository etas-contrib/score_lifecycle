import pytest
from os import environ, path
from pathlib import Path
from tests.utils.fixtures.file_interface.fixture import file_interface
import logging
from time import sleep
from typing import List


logger = logging.getLogger(__name__)

@pytest.fixture(scope="function")
def test_dir():
    return Path(environ["TEST_UNDECLARED_OUTPUTS_DIR"])

def remote_binary_paths() -> List[Path]:
    bin_paths = environ["SCORE_TEST_BINARY_PATH"]
    bin_paths = [Path(p) for p in bin_paths.split(' ')]
    root_path_index = list(bin_paths[-1].parts).index("opt")
    return [Path("/", *p.parts[root_path_index:]) for p in bin_paths]

@pytest.fixture(scope="function")
def download_test_results(file_interface, test_dir):
    remote_dir = environ["SCORE_TEST_REMOTE_DIRECTORY"]

    def _():
        for path, files in file_interface.walk(remote_dir):
            for file in files:
                if Path(file).suffix == ".xml":
                    remote_path = Path(path) / file
                    local = str(test_dir / remote_path.name )
                    logger.info(f"Downloading {file} to {local}")
                    file_interface.download(str(remote_path), str(local))

    return _

@pytest.fixture
def setup_tests(request, file_interface, control_interface):
    if request.config.getoption("--image-path") == "native":
        yield None
        return None

    bin_paths = environ["SCORE_TEST_BINARY_PATH"]
    bin_paths = [Path(p) for p in bin_paths.split(' ')]

    # get the local root path
    root_path_index = list(bin_paths[-1].parts).index("opt")

    for file in bin_paths:
        assert file.is_file() , f"{file} is not a file"
        remote_path = Path("/", *file.parts[root_path_index:])
        file_interface.upload(str(file), str(remote_path))
        ret, _, stderr = control_interface.exec_command_blocking(f"/proc/boot/chmod 777 {str(remote_path)}")
        assert ret == 0, f"Ret code: {ret}, {stderr}"

    logger.info("Test case setup finished")
