import os
import pytest
from pathlib import Path


def pytest_addoption(parser):
    parser.addoption(
        "--executable",
        action="store",
        help="Path to the of the clice executable."
    )
    parser.addoption(
        "--resource-dir",
        action="store",
        help="Path to the of the clang resource directory."
    )


@pytest.fixture(scope="session")
def executable(request):
    executable = request.config.getoption("--executable")
    if executable is None:
        pytest.exit(
            "Error: You must specify the 'clice' executable path using "
            "'--executable=<path/to/clice>' in pytest arguments, "
            "or configure it in your pytest.ini/conftest.py.",
            returncode=64
        )

    path = Path(executable)
    if not path.exists():
        pytest.exit(
            f"Error: 'clice' executable not found at '{executable}'. "
            "Please ensure the path is correct and the file exists.",
            returncode=64
        )

    return path.resolve()


@pytest.fixture(scope="session")
def resource_dir(request):
    path = request.config.getoption("--resource-dir")
    return Path(path).resolve()


@pytest.fixture(scope="session")
def test_data_dir(request):
    path = os.path.join(os.path.dirname(__file__), "data")
    return Path(path).resolve()
