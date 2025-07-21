import sys
import pytest
import asyncio
from ..fixtures.client import LSPClient


@pytest.mark.asyncio
async def test_initialize(executable, test_data_dir):
    client = LSPClient([
        executable, "--mode=pipe"
    ])
    await client.start()
    result = await client.initialize(test_data_dir)
    assert "serverInfo" in result
    assert result["serverInfo"]["name"] == "clice"
    await client.exit()
