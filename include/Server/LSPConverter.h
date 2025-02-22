#pragma once

#include "Config.h"
#include "Async/Async.h"
#include "Feature/Hover.h"
#include "Feature/InlayHint.h"
#include "Feature/FoldingRange.h"
#include "Feature/DocumentSymbol.h"
#include "Feature/SemanticTokens.h"

namespace clice {

/// Responsible for converting between LSP and internal types.
class LSPConverter {
public:
    using Result = async::Task<json::Value>;

    Result convert(llvm::StringRef path, llvm::ArrayRef<feature::SemanticToken> tokens);

    Result convert(const feature::Hover& hover);

private:
    proto::PositionEncodingKind kind;
};

}  // namespace clice

