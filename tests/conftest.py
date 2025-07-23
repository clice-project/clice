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
    executable = request.config.getoption("--executable")
    if executable is None:
        pytest.exit(
            "Error: You must specify the 'clice' executable path using "
            "'--executable=<path/to/clice>' in pytest arguments, "
            "or configure it in your pytest.ini/conftest.py.",
            returncode=os.EX_USAGE
        )

    path = Path(executable)
    if not path.exists():
        pytest.exit(
            f"Error: 'clice' executable not found at '{executable}'. "
            "Please ensure the path is correct and the file exists.",
            returncode=os.EX_USAGE
        )

    return path.resolve()


@pytest.fixture(scope="session")
def test_data_dir(request):
    path = os.path.join(os.path.dirname(__file__), "data")
    return Path(path).resolve()
