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
#include "Feature/CodeCompletion.h"
#include "Feature/SignatureHelp.h"

namespace clice {

enum class PositionEncodingKind : std::uint8_t {
    UTF8 = 0,
    UTF16,
    UTF32,
};

/// Responsible for converting between LSP and internal types.
class LSPConverter {
public:
    json::Value initialize(json::Value value);

    PositionEncodingKind encoding() {
        return kind;
    }

    llvm::StringRef workspace() {
        return workspacePath;
    }

public:
    /// Convert URI to file path with path mapping.
    std::string convert(llvm::StringRef URI);

    /// Convert a position into an offset relative to the beginning of the file.
    std::uint32_t convert(llvm::StringRef content, proto::Position position);

    proto::Position convert(llvm::StringRef content, std::uint32_t offset);

    json::Value convert(llvm::StringRef content, const feature::Hover& hover);

    json::Value convert(llvm::StringRef content, const feature::InlayHints& hints);

    json::Value convert(llvm::StringRef content, const feature::FoldingRanges& foldings);

    json::Value convert(llvm::StringRef content, const feature::DocumentLinks& links);

    json::Value convert(llvm::StringRef content, const feature::DocumentSymbols& symbols);

    json::Value convert(llvm::StringRef content, const feature::SemanticTokens& tokens);

    json::Value convert(llvm::StringRef content, const std::vector<feature::CompletionItem>& items);

private:
    PositionEncodingKind kind;
    std::string workspacePath;
};

}  // namespace clice

