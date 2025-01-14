#include "Basic/SourceConverter.h"
#include "Server/Server.h"

namespace clice {

async::Task<> Server::onInitialize(json::Value id, const proto::InitializeParams& params) {
    proto::InitializeResult result = {};
    result.serverInfo.name = "clice";
    result.serverInfo.version = "0.0.1";

    co_await response(std::move(id), json::serialize(result));
}

async::Task<> Server::onInitialized(const proto::InitializedParams& params) {
    co_return;
}

async::Task<> Server::onExit(const proto::None&) {
    co_return;
}

async::Task<> Server::onShutdown(json::Value id, const proto::None&) {
    co_return;
}

}  // namespace clice
