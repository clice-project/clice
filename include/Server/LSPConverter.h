#pragma once

#include "Config.h"
#include "Async/Async.h"
#include "Feature/Hover.h"
#include "Feature/InlayHint.h"
#include "Feature/FoldingRange.h"
#include "Feature/DocumentSymbol.h"
#include "Feature/SemanticToken.h"
#include "Feature/DocumentLink.h"
#include "Server/Protocol.h"

namespace clice {

/// Responsible for converting between LSP and internal types.
class LSPConverter {
public:
    using Result = async::Task<json::Value>;

    proto::InitializeResult initialize(json::Value value);

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
    uint32_t convert(llvm::StringRef content, proto::Position position);

    proto::SemanticTokens transform(llvm::StringRef content,
                                    llvm::ArrayRef<feature::SemanticToken> tokens);

    std::vector<proto::FoldingRange> transform(llvm::StringRef content,
                                               llvm::ArrayRef<feature::FoldingRange> foldings);

    std::vector<proto::DocumentLink> transform(llvm::StringRef content,
                                               llvm::ArrayRef<feature::DocumentLink> links);

private:
    proto::InitializeParams params;
    std::string workspacePath;
};

}  // namespace clice

