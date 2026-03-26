import pytest
from pathlib import Path
import logging


logger = logging.getLogger(__name__)


@pytest.fixture
def setup_test(request, target):
    """ Setsup the test by uploading all the binaries to the target
    """

    bin_paths = request.config.getoption("--score-test-binary-path")
    bin_paths = [Path(p) for p in bin_paths.split(" ")]

    root_path_index = list(bin_paths[-1].parts).index("opt")

    for file in bin_paths:
        assert file.is_file(), f"{file} is not a file"
        remote_path = Path("/", *file.parts[root_path_index:])

        logger.debug(target.execute(f"mkdir -p {remote_path.parent}"))
        logger.debug(target.execute(f"chmod 777 -R {remote_path.parent}"))

        target.upload(file.resolve(), remote_path)  # Need to resolve for https://github.com/eclipse-score/itf/pull/71

    logger.info("Test case setup finished")
