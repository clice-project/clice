#include "Basic/SourceConverter.h"
#include "Server/Server.h"

namespace clice {

async::promise<void> Server::onInitialize(json::Value id, const proto::InitializeParams& params) {
    auto workplace = SourceConverter::toPath(params.workspaceFolders[0].uri);
    config::init(workplace);

    if(!params.capabilities.workspace.didChangeWatchedFiles.dynamicRegistration) {
        log::fatal(
            "clice requires the client to support file event watching to monitor updates to CDB files");
    }

    proto::InitializeResult result = {};
    async::response(std::move(id), json::serialize(result));

    /// Load the compile commands from the workspace.
    for(auto dir: config::server.compile_commands_dirs) {
        synchronizer.sync(dir + "/compile_commands.json");
    }

    co_return;
}

async::promise<void> Server::onInitialized(const proto::InitializedParams& params) {
    proto::DidChangeWatchedFilesRegistrationOptions options;
    for(auto& dir: config::server.compile_commands_dirs) {
        options.watchers.emplace_back(proto::FileSystemWatcher{
            dir + "/compile_commands.json",
        });
    }
    async::registerCapacity("watchedFiles",
                            "workspace/didChangeWatchedFiles",
                            json::serialize(options));

    /// Load all information about PCHs and PCMs from disk.
    scheduler.loadCache();
    co_return;
}

async::promise<void> Server::onExit(const proto::None&) {
    co_return;
}

async::promise<void> Server::onShutdown(json::Value id, const proto::None&) {
    /// Save all information about PCHs and PCMs to disk.
    scheduler.saveCache();
    co_return;
}

}  // namespace clice
