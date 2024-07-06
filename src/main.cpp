#include <LSP/Server.h>
#include <LSP/Protocol.h>
#include <Support/Logger.h>
#include <Support/Serialize.h>

int main(int argc, char** argv) {
    clice::logger::init(argv[0]);
    auto& server = clice::server;
    clice::logger::info("Starting server...");
    server.start();
    return 0;
}
