#include "../Language/SemanticToken.h"

namespace clice::protocol {

struct ServerCapabilities {
    std::string_view positionEncoding = "utf-16";
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
