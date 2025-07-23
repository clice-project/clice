from pathlib import Path
from .transport import LSPTransport


class LSPClient(LSPTransport):
    def __init__(self, commands, mode="stdio", host="127.0.0.1", port=2087):
        super().__init__(commands, mode, host, port)
        self.workspace = ""
        self.opening_files: dict[Path, str] = {}

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

    def get_abs_path(self, relative_path):
        return Path(self.workspace, relative_path)

    async def did_open(self, relative_path: str):
        path = self.get_abs_path(relative_path)

        content = ""
        with open(path, encoding="utf-8") as file:
            content = file.read()

        if path in self.opening_files:
            raise f"Cannot open same file multiple times: {path}"

        self.opening_files[path] = content

        params = {
            "textDocument": {
                "uri": path.as_uri(),
                "languageId": "cpp",
                "version": 0,
                "text": content,
            }
        }

        await self.send_notification("textDocument/didOpen", params)
