#pragma once

#include "Config.h"
#include "Async/Async.h"
#include "Feature/Hover.h"
#include "Feature/InlayHint.h"
#include "Feature/FoldingRange.h"
#include "Feature/DocumentSymbol.h"
#include "Feature/SemanticTokens.h"
#include "Server/Protocol.h"

namespace clice {

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
    Result convert(llvm::StringRef path, llvm::ArrayRef<feature::SemanticToken> tokens);

    Result convert(llvm::StringRef path, llvm::ArrayRef<feature::FoldingRange> foldings);

    Result convert(const feature::Hover& hover);

private:
    proto::InitializeParams params;
    std::string workspacePath;
};

}  // namespace clice

