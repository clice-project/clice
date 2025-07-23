import sys
import pytest
import asyncio
from ..fixtures.client import LSPClient


@pytest.mark.asyncio
async def test_did_open(executable, test_data_dir):
    client = LSPClient([
        executable, "--mode=pipe"
    ])
    await client.start()

    await client.initialize(test_data_dir / "hello_world")
    await client.did_open("main.cpp")
    await asyncio.sleep(3)
    await client.exit()
