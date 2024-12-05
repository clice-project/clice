#include "Server/Server.h"

namespace clice {

async::promise<void> Server::onInitialize(json::Value id, const proto::InitializeParams& params) {
    auto workplace = URI::resolve(params.workspaceFolders[0].uri);
    config::init(workplace);

    proto::InitializeResult result = {};
    async::write(std::move(id), json::serialize(result));
    co_return;
}

async::promise<void> Server::onInitialized(const proto::InitializedParams& params) {
    co_return;
}

async::promise<void> Server::onExit(const proto::None&) {
    co_return;
}

async::promise<void> Server::onShutdown(json::Value id, const proto::None&) {
    co_return;
}

}  // namespace clice
