#include <LSP/Server.h>
#include <LSP/Protocol.h>
#include <Support/Logger.h>
#include <Support/Serialize.h>
#include <Support/Async.h>

using namespace clice;

Task<int> foo() {
    int n = 0;
    int x = co_await async([]() -> int {
        logger::info("Sleeping for 10 seconds...");
        std::this_thread::sleep_for(std::chrono::seconds(10));
        return 10;
    });
    co_return x;
}

Task<void> bar() {
    int x = co_await foo();
    logger::info("x = {}", x);
    co_return;
}

int main(int argc, char** argv) {
    try {
        clice::logger::init(argv[0]);
        auto& server = clice::server;
        clice::logger::info("Starting server...");
        auto c = bar();
        c.resume();
        server.run();
    } catch(std::exception& e) {
        clice::logger::error("Failed to start server: {}", e.what());
        return 1;
    }
    return 0;
}
