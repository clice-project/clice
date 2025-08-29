#include "Server/Server.h"

namespace clice {

namespace {

/// Load compile commands from given directories. If no valid commands are found,
/// search recursively from the workspace directory.
void load_compile_commands(CompilationDatabase& database,
                           const std::vector<std::string>& compile_commands_dirs,
                           llvm::StringRef workspace) {

    auto try_load = [&database, workspace](llvm::StringRef dir) {
        std::string filepath = path::join(dir, "compile_commands.json");
        auto content = fs::read(filepath);
        if(!content) {
            log::warn("Failed to read CDB file: {}, {}", filepath, content.error());
            return false;
        }

        auto load = database.load_commands(*content, workspace);
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

    std::error_code ec;
    for(fs::recursive_directory_iterator it(workspace, ec), end; it != end && !ec;
        it.increment(ec)) {
        auto status = it->status();
        if(!status) {
            continue;
        }

        // Skip hidden directories.
        llvm::StringRef filename = path::filename(it->path());
        if(fs::is_directory(*status) && filename.starts_with('.')) {
            it.no_push();
            continue;
        }

        if(fs::is_regular_file(*status) && filename == "compile_commands.json") {
            if(try_load(path::parent_path(it->path()))) {
                return;
            }
        }
    }

    /// TODO: Add a default command in clice.toml. Or load commands from .clangd ?
    log::warn("Can not found any valid CDB file in current workspace, fallback to default mode.");
}

}  // namespace

async::Task<json::Value> Server::on_initialize(proto::InitializeParams params) {
    log::info("Initialize from client: {}, version: {}",
              params.clientInfo.name,
              params.clientInfo.version);

    /// FIXME: adjust position encoding.
    kind = PositionEncodingKind::UTF16;
    workspace = mapping.to_path(([&] -> std::string {
        if(params.workspaceFolders && !params.workspaceFolders->empty()) {
            return params.workspaceFolders->front().uri;
        }
        if(params.rootUri) {
            return *params.rootUri;
        }

        log::fatal("The client should provide one workspace folder or rootUri at least!");
    })());

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
