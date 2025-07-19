import pytest
from pathlib import Path


def pytest_addoption(parser):
    """
    定义自定义命令行选项。
    """
    parser.addoption(
        "--test-data-dir",
        action="store",
        default="tests/data",
        help="Path to the directory containing test data files."
    )
    parser.addoption(
        "--config-dir",
        action="store",
        default="configs",
        help="Path to the directory containing configuration files."
    )


@pytest.fixture(scope="session")
def test_data_dir(request):
    path_str = request.config.getoption("--test-data-dir")
    return Path(path_str).resolve()


@pytest.fixture(scope="session")
def config_dir(request):
    path_str = request.config.getoption("--config-dir")
    return Path(path_str).resolve()
