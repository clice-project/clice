#include "../Language/SemanticToken.h"

namespace clice::protocol {

enum class TextDocumentSyncKind {
    None = 0,
    Full = 1,
    Incremental = 2,
};

struct ServerCapabilities {
    std::string_view positionEncoding = "utf-16";
    TextDocumentSyncKind textDocumentSync = TextDocumentSyncKind::Full;
    SemanticTokensOptions semanticTokensProvider;
};

struct InitializeResult {
    ServerCapabilities capabilities;

    struct {
        std::string_view name = "clice";
        std::string_view version = "0.0.1";
    } serverInfo;
};

}  // namespace clice::protocol
