#include "Basic/Basic.h"
#include "Server/Server.h"

using namespace clice;

int main(int argc, const char** argv) {
    LSPServer LSP;
    auto loop = [&LSP](json::Value value, Writer& writer) -> promise<void> {
        co_await LSP.dispatch(std::move(value), writer);
    };
    Server server(loop, "127.0.0.1", 50051);
    server.run();
    return 0;
}

