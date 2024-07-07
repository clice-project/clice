#include <LSP/Server.h>
#include <LSP/Protocol.h>
#include <LSP/Transport.h>

#include <Support/Logger.h>
#include <Support/Serialize.h>

namespace clice {

Server server;

int Server::run() {
    loop = uv_default_loop();

    transport = std::make_unique<Pipe>();

    // start the event loop
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
        }
    } catch(std::exception& e) {
        logger::error("failed to parse JSON: {}", e.what());
        return;
    }
}

}  // namespace clice
