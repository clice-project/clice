#pragma once

#include "Feature/SemanticToken.h"
#include "Feature/CodeCompletion.h"

namespace clice {

enum class PositionEncodingKind : std::uint8_t {
    UTF8 = 0,
    UTF16,
    UTF32,
};

namespace proto {

std::string to_json(PositionEncodingKind kind,
                    llvm::StringRef content,
                    llvm::ArrayRef<feature::SemanticToken> tokens);

std::string to_json(PositionEncodingKind kind,
                    llvm::StringRef content,
                    llvm::ArrayRef<feature::CompletionItem> items);

}  // namespace proto

}  // namespace clice
