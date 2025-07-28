#pragma once

#include "Protocol/Protocol.h"
#include "Feature/SemanticToken.h"
#include "Feature/CodeCompletion.h"
#include "Support/JSON.h"

namespace clice::proto {

/// Convert a proto::Position to a file offset in the content with the specified encoding kind.
std::uint32_t to_offset(PositionEncodingKind kind,
                        llvm::StringRef content,
                        proto::Position position);

json::Value to_json(PositionEncodingKind kind,
                    llvm::StringRef content,
                    llvm::ArrayRef<feature::SemanticToken> tokens);
json::Value to_json(PositionEncodingKind kind,
                    llvm::StringRef content,
                    llvm::ArrayRef<feature::CompletionItem> items);

}  // namespace clice::proto
