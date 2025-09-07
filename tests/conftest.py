import os
import pytest
import logging
import pytest_asyncio
from pathlib import Path
from .fixtures.client import LSPClient


def pytest_addoption(parser):
    parser.addoption(
        "--executable",
        action="store",
        help="Path to the of the clice executable.",
    )
    parser.addoption(
        "--resource-dir",
        action="store",
        help="Path to the of the clang resource directory.",
    )


@pytest.fixture(scope="session")
def executable(request):
    executable = request.config.getoption("--executable")
    if executable is None:
        pytest.exit(
            "Error: You must specify the 'clice' executable path using "
            "'--executable=<path/to/clice>' in pytest arguments, "
            "or configure it in your pytest.ini/conftest.py.",
            returncode=64,
        )

    path = Path(executable)
    if not path.exists():
        pytest.exit(
            f"Error: 'clice' executable not found at '{executable}'. "
            "Please ensure the path is correct and the file exists.",
            returncode=64,
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


@pytest_asyncio.fixture(scope="function")
async def client(request, executable: Path, resource_dir: Path, test_data_dir: Path):
    cmd = [
        str(executable),
        "--mode=pipe",
        f"--resource-dir={resource_dir}",
    ]

    if hasattr(request, "param") and request.param:
        if "config_project" in request.param:
            project_name = request.param["config_project"]
            config_path = test_data_dir / project_name / "clice.toml"
            cmd.append(f"--config={config_path}")

    lsp_client = LSPClient(cmd)
    await lsp_client.start()

    yield lsp_client

    try:
        await lsp_client.exit()
    except Exception as e:
        logging.error(f"Error during LSP client exit: {e}")
