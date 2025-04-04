#pragma once

#include "Config.h"
#include "Protocol.h"
#include "Async/Async.h"
#include "Feature/Hover.h"
#include "Feature/InlayHint.h"
#include "Feature/FoldingRange.h"
#include "Feature/DocumentLink.h"
#include "Feature/DocumentSymbol.h"
#include "Feature/SemanticToken.h"

namespace clice {

struct InitializeParams;

/// Responsible for converting between LSP and internal types.
class LSPConverter {
public:
    using Result = async::Task<json::Value>;

    json::Value initialize(json::Value value);

    auto encoding() {
        return params.capabilities.general.positionEncodings[0];
    }

    auto& capabilities() {
        return params.capabilities;
    }

    /// The path of the workspace.
    llvm::StringRef workspace();

public:
    /// Convert a position into an offset relative to the beginning of the file.
    std::uint32_t convert(llvm::StringRef content, proto::Position position);

    /// Convert `TextDocumentParams` to file path.
    std::string convert(proto::TextDocumentParams params);

    json::Value convert(llvm::StringRef content, const feature::FoldingRanges& foldings);

    json::Value convert(llvm::StringRef content, const feature::DocumentLinks& links);

    json::Value convert(llvm::StringRef content, const feature::DocumentSymbols& symbols);

    json::Value convert(llvm::StringRef content, const feature::SemanticTokens& tokens);

private:
    proto::InitializeParams params;
    std::string workspacePath;
};

}  // namespace clice

