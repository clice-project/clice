#include <LSP/Server.h>
#include <LSP/Setting.h>
#include <LSP/Protocol.h>
#include <LSP/Transport.h>

#include <Support/Logger.h>
#include <Support/Serialize.h>

namespace clice {

Server server;

int Server::run(int argc, char** argv) {
    assert(argc == 2);
    logger::init(argv[0]);

    logger::info("reading settings...");
    setting = deserialize<Setting>(json::parse(argv[1]));
    logger::info("settings: {}", argv[1]);

    loop = uv_default_loop();

    if(setting.mode == "pipe") {
        transport = std::make_unique<Pipe>();
    } else if(setting.mode == "socket") {
        // transport = std::make_unique<Socket>();
    } else if(setting.mode == "index") {
        transport = {};
    } else {
        logger::error("invalid mode: {}", setting.mode);
        return 1;
    }

    // start the event loop
    logger::info("starting the server...");
    return uv_run(loop, UV_RUN_DEFAULT);
}

int Server::exit() {
    // stop the event loop
    uv_stop(loop);
    return 0;
}

void Server::handle_message(std::string_view message) {
    try {
        auto input = json::parse(message);
        std::string_view method = input["method"].get<std::string_view>();
        if(method == "initialize") {
            int id = input["id"].get<int>();
            // initialize();
            json json = {
                {"id",     id                                  },
                {"result", clice::serialize(InitializeResult{})}
            };
            transport->send(json.dump());
        } else if(method == "textDocument/didOpen") {
            // didOpen();
            auto params = deserialize<DidOpenTextDocumentParams>(input["params"]);
            logger::info("serialize successfully: {}", serialize(params).dump());
            auto task = didOpen(params);
            task.resume();
        }
    } catch(std::exception& e) {
        logger::error("failed to parse JSON: {}", e.what());
        return;
    }
}

Task<void> Server::didOpen(const DidOpenTextDocumentParams& params) {
    logger::info("textDocument/didOpen: {}", params.textDocument.uri);
    auto& textDocument = params.textDocument;
    co_await scheduler.update(textDocument.uri, textDocument.text);
}

}  // namespace clice
