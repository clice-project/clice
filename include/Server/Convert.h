#pragma once

#include "Protocol/Protocol.h"
#include "Feature/SemanticToken.h"
#include "Feature/CodeCompletion.h"

namespace clice::proto {

std::string to_json(PositionEncodingKind kind,
                    llvm::StringRef content,
                    llvm::ArrayRef<feature::SemanticToken> tokens);

std::string to_json(PositionEncodingKind kind,
                    llvm::StringRef content,
                    llvm::ArrayRef<feature::CompletionItem> items);

}  // namespace clice::proto
