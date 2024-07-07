#include <LSP/Server.h>
#include <LSP/Protocol.h>
#include <Support/Logger.h>
#include <Support/Serialize.h>

int main(int argc, char** argv) {
    try {
        clice::logger::init(argv[0]);
        auto& server = clice::server;
        clice::logger::info("Starting server...");
        server.run();
    } catch(std::exception& e) {
        clice::logger::error("Failed to start server: {}", e.what());
        return 1;
    }
    return 0;
}
