#include "Server/Server.h"

#include <filesystem>

namespace clice {

namespace {

namespace stdfs = std::filesystem;

/// Load compile commands from given directories. If no valid commands are found,
/// search recursively from the workspace directory.
void load_compile_commands(CompilationDatabase& database,
                           const std::vector<std::string>& compile_commands_dirs,
                           const std::string& workspace) {
    auto try_load = [&database](const std::string& dir) {
        std::string filepath = dir + "/compile_commands.json";
        auto content = fs::read(filepath);
        if(!content) {
            log::warn("Failed to read CDB file: {}, {}", filepath, content.error());
            return false;
        }

        auto load = database.load_commands(*content);
        if(!load) {
            log::warn("Failed to load CDB file: {}. {}", filepath, load.error());
            return false;
        }

        log::info("Load CDB file: {} successfully, {} items loaded", filepath, load->size());
        return true;
    };

    if(std::ranges::any_of(compile_commands_dirs, try_load)) {
        return;
    }

    log::info(
        "Can not found any valid CDB file from given directories, search recursively from workspace: {} ...",
        workspace);

    auto recursive_search = [&try_load](this auto&& self, const stdfs::path& dir) -> bool {
        if(!stdfs::exists(dir) || !stdfs::is_directory(dir)) {
            return false;
        }

        // Skip hidden directories.
        if(dir.filename().string().starts_with('.')) {
            return false;
        }

        if(try_load(dir.string())) {
            return true;
        }

        for(const auto& entry: stdfs::directory_iterator(dir)) {
            if(self(entry.path())) {
                return true;
            }
        }

        return false;
    };

    if(recursive_search(workspace)) {
        return;
    }

    /// TODO: Add a default command in clice.toml. Or load commands from .clangd ?
    log::warn("Can not found any valid CDB file in current workspace, fallback to default mode.");
}

}  // namespace

async::Task<json::Value> Server::on_initialize(proto::InitializeParams params) {
    log::info("Initialize from client: {}, version: {}",
              params.clientInfo.name,
              params.clientInfo.version);

    if(params.workspaceFolders.empty()) {
        log::fatal("The client should provide one workspace folder at least!");
    }

    /// FIXME: adjust position encoding.
    kind = PositionEncodingKind::UTF16;
    workspace = mapping.to_path(params.workspaceFolders[0].uri);

    /// Initialize configuration.
    config::init(workspace);

    /// Set server options.
    opening_files.set_capability(config::server.max_active_file);

    /// Load compile commands.json
    load_compile_commands(database, config::server.compile_commands_dirs, workspace);

    /// Load cache info.
    load_cache_info();

    proto::InitializeResult result;
    auto& [info, capabilities] = result;
    info.name = "clice";
    info.version = "0.0.1";

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

    /// Hover
    capabilities.hoverProvider = true;

    /// SignatureHelp
    capabilities.signatureHelpProvider.triggerCharacters = {"(", ")", "{", "}", "<", ">", ","};

    /// DocumentSymbol
    capabilities.documentSymbolProvider = {};

    /// DocumentLink
    capabilities.documentLinkProvider.resolveProvider = false;

    /// Formatting
    capabilities.documentFormattingProvider = true;
    capabilities.documentRangeFormattingProvider = true;

    /// FoldingRange
    capabilities.foldingRangeProvider = true;

    /// Semantic tokens
    capabilities.semanticTokensProvider.range = false;
    capabilities.semanticTokensProvider.full = true;
    for(auto name: SymbolKind::all()) {
        std::string type{name};
        type[0] = std::tolower(type[0]);
        capabilities.semanticTokensProvider.legend.tokenTypes.emplace_back(std::move(type));
    }

    /// Inlay hint
    /// FIXME: Resolve to make hint clickable.
    capabilities.inlayHintProvider.resolveProvider = false;

    co_return json::serialize(result);
}

async::Task<> Server::on_initialized(proto::InitializedParams) {
    co_return;
}

async::Task<json::Value> Server::on_shutdown(proto::ShutdownParams params) {
    co_return json::Value(nullptr);
}

async::Task<> Server::on_exit(proto::ExitParams params) {
    save_cache_info();
    async::stop();
    co_return;
}

}  // namespace clice
