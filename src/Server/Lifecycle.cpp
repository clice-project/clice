#include "Basic/SourceConverter.h"
#include "Server/Server.h"

namespace clice {

async::Task<> Server::onInitialize(json::Value id, const proto::InitializeParams& params) {
    co_return;
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
