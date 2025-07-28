#include "Server/Server.h"

namespace clice {

async::Task<json::Value> Server::on_initialize(proto::InitializeParams params) {
    log::info("Initialize from client: {}, version: {}",
              params.clientInfo.name,
              params.clientInfo.verion);

    if(params.workspaceFolders.empty()) {
        log::fatal("The client should provide one workspace folder at least!");
    }

    // auto result = converter.initialize(std::move(value));
    // config::init(converter.workspace());
    //
    // for(auto& dir: config::server.compile_commands_dirs) {
    //    auto content = fs::read(dir + "/compile_commands.json");
    //    if(content) {
    //        auto updated = database.load_commands(*content);
    //    }
    //}

    proto::InitializeResult result;
    result.serverInfo.name = "clice";
    result.serverInfo.verion = "0.0.1";

    co_return json::serialize(result);
}

}  // namespace clice
