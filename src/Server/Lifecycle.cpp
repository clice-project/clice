#include "Server/Server.h"

namespace clice {

async::Task<json::Value> Server::on_initialize(proto::InitializeParams params) {
    log::info("Initialize from client: {}, version: {}",
              params.clientInfo.name,
              params.clientInfo.verion);

    if(params.workspaceFolders.empty()) {
        log::fatal("The client should provide one workspace folder at least!");
    }

    /// FIXME: adjust position encoding.
    kind = PositionEncodingKind::UTF16;
    workspace = mapping.to_path(params.workspaceFolders[0].uri);

    /// Initialize configuration.
    config::init(workspace);

    /// Load compile commands.json
    for(auto& dir: config::server.compile_commands_dirs) {
        auto content = fs::read(dir + "/compile_commands.json");
        if(content) {
            auto updated = database.load_commands(*content);
        }
    }

    proto::InitializeResult result;
    auto& [info, capabilities] = result;
    info.name = "clice";
    info.verion = "0.0.1";

    capabilities.positionEncoding = "utf-16";

    /// TextDocument synchronization.
    capabilities.textDocumentSync.openClose = true;
    /// FIXME: In the end, we should use `Incremental`.
    capabilities.textDocumentSync.change = proto::TextDocumentSyncKind::Full;
    capabilities.textDocumentSync.save = true;

    /// Completion
    capabilities.completionProvider.triggerCharacters = {".", "<", ">", ":", "\"", "/", "*"};
    capabilities.completionProvider.resolveProvider = false;
    capabilities.completionProvider.completionItem.labelDetailsSupport = true;

    /// Semantic tokens.
    capabilities.semanticTokensProvider.range = false;
    capabilities.semanticTokensProvider.full = true;
    for(auto name: SymbolKind::all()) {
        std::string type{name};
        type[0] = std::tolower(type[0]);
        capabilities.semanticTokensProvider.legend.tokenTypes.emplace_back(std::move(type));
    }

    co_return json::serialize(result);
}

}  // namespace clice
