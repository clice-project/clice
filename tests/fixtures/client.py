from pathlib import Path
from .transport import LSPTransport


class LSPClient(LSPTransport):
    def __init__(self, commands, mode="stdio", host="127.0.0.1", port=2087):
        super().__init__(commands, mode, host, port)

    async def initialize(self, workspace: str):
        self.workspace = workspace
        params = {
            "clientInfo": {"name": "clice tester", "version": "0.0.1", },
            "capabilities": {},
            "workspaceFolders": [{"uri": Path(workspace).as_uri(), "name": "test"}],
        }
        return await self.send_request("initialize", params)

    async def exit(self):
        await self.send_notification("exit")
        await self.stop()
