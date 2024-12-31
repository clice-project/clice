#pragma once

#include <Basic/Location.h>
#include <clang/Basic/SourceLocation.h>

namespace clice {

/// Measure the length of the content with the specified encoding kind.
std::size_t remeasure(llvm::StringRef content, proto::PositionEncodingKind kind);

/// Convert a clang::SourceLocation to a proto::Position according to the
/// specified encoding kind. Note that `SourceLocation` in clang is 1-based and
/// is always encoded in UTF-8.
proto::Position toPosition(llvm::StringRef content, clang::SourceLocation location,
                           proto::PositionEncodingKind kind, const clang::SourceManager& SM);

/// Same as above, but content is retrieved from the `SourceManager`.
proto::Position toPosition(clang::SourceLocation location, proto::PositionEncodingKind kind,
                           const clang::SourceManager& SM);

/// Convert a proto::Position to a file offset in the content with the specified
/// encoding kind.
std::size_t toOffset(llvm::StringRef content, proto::Position position,
                     proto::PositionEncodingKind kind);

/// A helper class to convert "position" between 1-1 based Clang `SourceLocation` and LSP 0-0 based
/// `proto::Position`.
class SourceConverter {
public:
    /// Construct a `SourceConverter` with the specified encoding kind.
    explicit SourceConverter(proto::PositionEncodingKind kind) : kind(kind) {}

    /// Convert a `SourceLocation` to a `Position`.
    proto::Position toPosition(llvm::StringRef content, clang::SourceLocation location,
                               const clang::SourceManager& SM) const {
        return clice::toPosition(content, location, kind, SM);
    }

    /// Convert a `SourceLocation` to a `Position`.
    proto::Position toPosition(clang::SourceLocation location,
                               const clang::SourceManager& SM) const {
        return clice::toPosition(location, kind, SM);
    }

    /// Convert a `Position` to a file offset.
    std::size_t toOffset(llvm::StringRef content, proto::Position position) const {
        return clice::toOffset(content, position, kind);
    }

    /// Convert a `Position` to a file offset.
    std::size_t toOffset(llvm::StringRef content, const proto::Position& position) const {
        return clice::toOffset(content, position, kind);
    }

    /// Get the encoding kind of the content in LSP protocol.
    proto::PositionEncodingKind encodingKind() const {
        return kind;
    }

private:
    /// The encoding kind of the content in LSP protocol.
    proto::PositionEncodingKind kind;
};

}  // namespace clice
