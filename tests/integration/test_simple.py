import sys
import pytest
import asyncio
from ..fixtures.client import LSPClient


@pytest.mark.asyncio
async def test_initialize():
    client = LSPClient([
        "/home/ykiko/C++/clice/build/bin/clice",
        "--resource-dir=/home/ykiko/C++/llvm-project/build-debug-install/lib/clang/20",
        "--mode=pipe",
    ])
    await client.start()
    result = await client.initialize("/home/ykiko/C++/clice")
    assert "serverInfo" in result
    assert result["serverInfo"]["name"] == "clice"
    await client.exit()
