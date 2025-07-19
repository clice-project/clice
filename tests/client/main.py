import asyncio
from client import LSPClient


async def main():
    client = LSPClient([
        "/home/ykiko/C++/clice/build/bin/clice",
        "--resource-dir=/home/ykiko/C++/llvm-project/build-debug-install/lib/clang/20",
        "--mode=pipe",
    ])
    await client.start()
    await client.initialize("/home/ykiko/C++/clice")
    await client.exit()

if __name__ == "__main__":
    asyncio.run(main())
