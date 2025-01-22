#pragma once

#include "Basic/Document.h"
#include "Basic/SymbolKind.h"
#include "Basic/SourceCode.h"
#include "Basic/SourceConverter.h"
#include "Index/Shared.h"

namespace clice {

class ASTInfo;

namespace config {

struct SemanticTokensOption {};

};  // namespace config

namespace proto {

/// Server Capability.
struct SemanticTokensOptions {
    /// The legend used by the server.
    struct SemanticTokensLegend {
        /// The token types a server uses.
        std::vector<std::string> tokenTypes;

        /// The token modifiers a server uses.
        std::vector<std::string> tokenModifiers;
    } legend;

    /// Server supports providing semantic tokens for a specific range
    /// of a document.
    bool range = false;

    /// Server supports providing semantic tokens for a full document.
    bool full = true;
};

struct SemanticTokensParams {
    /// The text document.
    TextDocumentIdentifier textDocument;
};

struct SemanticTokens {
    /// The actual tokens.
    std::vector<uinteger> data;
};

}  // namespace proto

namespace feature {

struct SemanticToken {
    LocalSourceRange range;
    SymbolKind kind;
    SymbolModifiers modifiers;
};

/// Generate semantic tokens for all files.
index::Shared<std::vector<SemanticToken>> semanticTokens(ASTInfo& info);

/// Translate semantic tokens to LSP format.
proto::SemanticTokens toSemanticTokens(llvm::ArrayRef<SemanticToken> tokens,
                                       SourceConverter& SC,
                                       llvm::StringRef content,
                                       const config::SemanticTokensOption& option);

/// Generate semantic tokens for main file and translate to LSP format.
proto::SemanticTokens semanticTokens(ASTInfo& info,
                                     SourceConverter& SC,
                                     const config::SemanticTokensOption& option);

}  // namespace feature

}  // namespace clice

