import sys
import pytest
import asyncio
import logging
from ..fixtures.client import LSPClient


@pytest.mark.asyncio
async def test_did_open(executable, test_data_dir, resource_dir):
    client = LSPClient([
        executable, "--mode=pipe", f"--resource-dir={resource_dir}"
    ])
    await client.start()

    await client.initialize(test_data_dir / "hello_world")
    await client.did_open("main.cpp")
    await asyncio.sleep(5)
    await client.exit()


@pytest.mark.asyncio
async def test_did_change(executable, test_data_dir, resource_dir):
    client = LSPClient([
        executable, "--mode=pipe", f"--resource-dir={resource_dir}"
    ])
    await client.start()

    await client.initialize(test_data_dir / "hello_world")
    await client.did_open("main.cpp")

    # Test frequently change content will not make server crash.
    content = client.get_file("main.cpp").content
    for _ in range(0, 4):
        content += "\n"
        await client.did_change("main.cpp", content)
        await asyncio.sleep(0.1)

    await asyncio.sleep(5)
    await client.exit()
