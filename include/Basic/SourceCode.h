#pragma once

#include <Basic/Location.h>
#include <clang/Basic/SourceLocation.h>

namespace clice {

/// Measure the length of the content with the specified encoding kind.
std::size_t remeasure(llvm::StringRef content, proto::PositionEncodingKind kind);

/// Convert a clang::SourceLocation to a proto::Position according to the
/// specified encoding kind. Note that `SourceLocation` in clang is 1-based and
/// is always encoded in UTF-8.
proto::Position toPosition(llvm::StringRef content,
                           clang::SourceLocation location,
                           proto::PositionEncodingKind kind,
                           const clang::SourceManager& SM);

/// Same as above, but content is retrieved from the `SourceManager`.
proto::Position toPosition(clang::SourceLocation location,
                           proto::PositionEncodingKind kind,
                           const clang::SourceManager& SM);

/// Convert a proto::Position to a file offset in the content with the specified
/// encoding kind.
std::size_t toOffset(llvm::StringRef content,
                     proto::Position position,
                     proto::PositionEncodingKind kind);

}  // namespace clice
