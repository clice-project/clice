#include "Server/Server.h"
#include "Support/FileSystem.h"

namespace clice {

async::Task<> Server::initialize(json::Value value) {
    converter.initialize(std::move(value));

    proto::InitializeResult result = {};

    /// Initialize the server info.
    result.serverInfo.name = "clice";
    result.serverInfo.version = "0.0.1";

    co_await response(std::move(id), json::serialize(result));
    co_return;
}

// async::Task<> Server::onInitialize(json::Value id, const proto::InitializeParams& params) {
//     proto::InitializeResult result = {};
//     result.serverInfo.name = "clice";
//     result.serverInfo.version = "0.0.1";
//
//     /// Set `SemanticTokensOptions`
//     for(auto kind: SymbolKind::all()) {
//         std::string name{kind};
//         name[0] = std::tolower(name[0]);
//         result.capabilities.semanticTokensProvider.legend.tokenTypes.emplace_back(std::move(name));
//     }
//
//     result.capabilities.semanticTokensProvider.legend.tokenModifiers = {
//
//     };
//
//     co_await response(std::move(id), json::serialize(result));
//
//     auto workplace = SourceConverter::toPath(params.workspaceFolders[0].uri);
//     config::init(workplace);
//
//     for(auto& dir: config::server.compile_commands_dirs) {
//         llvm::SmallString<128> path = {dir};
//         path::append(path, "compile_commands.json");
//         database.updateCommands(path);
//     }
// }

/// async::Task<> Server::onInitialized(const proto::InitializedParams& params) {
///     co_return;
/// }
///
/// async::Task<> Server::onExit(const proto::None&) {
///     co_return;
/// }
///
/// async::Task<> Server::onShutdown(json::Value id, const proto::None&) {
///     co_return;
/// }

}  // namespace clice
