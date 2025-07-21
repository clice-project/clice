import os
import pytest
from pathlib import Path


def pytest_addoption(parser):
    parser.addoption(
        "--executable",
        action="store",
        help="Path to the of the clice executable."
    )


@pytest.fixture(scope="session")
def executable(request):
    path = request.config.getoption("--executable")
    return Path(path).resolve()


@pytest.fixture(scope="session")
def test_data_dir(request):
    path = os.path.join(os.path.dirname(__file__), "data")
    return Path(path).resolve()
