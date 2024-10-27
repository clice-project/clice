#pragma once

#include <Basic/Location.h>
#include <clang/Basic/SourceLocation.h>

namespace clice {

/// Convert a clang::SourceLocation to a proto::Position according to the
/// specified encoding kind. Note that `SourceLocation` in clang is one-based and
/// is always encoded in UTF-8.
proto::Position toPosition(llvm::StringRef content,
                           clang::SourceLocation location,
                           proto::PositionEncodingKind kind,
                           const clang::SourceManager& srcMgr);

/// Same as above, but for a group of locations. It is more efficient than calling
/// `toLocation` multiple times. Note that the locations must be sorted.
std::vector<proto::Position> toPosition(llvm::StringRef content,
                                        llvm::ArrayRef<clang::SourceLocation> locations,
                                        proto::PositionEncodingKind kind,
                                        const clang::SourceManager& srcMgr);

/// Convert a proto::Position to a clang::SourceLocation according to the
/// specified encoding kind. If any error occurs, return an invalid location.
clang::SourceLocation toSourceLocation(llvm::StringRef content,
                                       proto::Position position,
                                       proto::PositionEncodingKind kind,
                                       const clang::SourceManager& srcMgr);

}  // namespace clice
