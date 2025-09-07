import pytest


@pytest.mark.asyncio
async def test_initialize(client, test_data_dir):
    result = await client.initialize(test_data_dir)
    assert "serverInfo" in result
    assert result["serverInfo"]["name"] == "clice"
