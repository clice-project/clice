#pragma once

#include <Basic/Location.h>
#include <clang/Basic/SourceLocation.h>

namespace clice {

/// A helper class to convert `Position, Range, Location` between 1-1 based clang and 0-0 based
/// LSP.
class SourceConverter {
public:
    /// Construct a `SourceConverter` with the specified encoding kind.
    explicit SourceConverter(proto::PositionEncodingKind kind) : kind(kind) {}

    /// Measure the length of the content with the specified encoding kind.
    static std::size_t remeasure(llvm::StringRef content, proto::PositionEncodingKind kind);

    /// Measure the length of the content with the specified encoding kind.
    std::size_t remeasure(llvm::StringRef content) const {
        return remeasure(content, kind);
    }

    /// Convert a clang::SourceLocation to a proto::Position according to the
    /// specified encoding kind. Note that `SourceLocation` in clang is 1-based and
    /// is always encoded in UTF-8.
    proto::Position toPosition(llvm::StringRef content, clang::SourceLocation location,
                               const clang::SourceManager& SM) const;

    /// Same as above, but content is retrieved from the `SourceManager`.
    proto::Position toPosition(clang::SourceLocation location,
                               const clang::SourceManager& SM) const;

    /// Convert a clang::SourceRange to a proto::Range according to the specified encoding kind.
    proto::Range toRange(clang::SourceRange range, const clang::SourceManager& SM) const {
        return {toPosition(range.getBegin(), SM), toPosition(range.getEnd(), SM)};
    }

    /// Convert a proto::Position to a file offset in the content with the specified
    /// encoding kind.
    std::size_t toOffset(llvm::StringRef content, proto::Position position) const;

    /// Get the encoding kind of the content in LSP protocol.
    proto::PositionEncodingKind encodingKind() const {
        return kind;
    }

private:
    /// The encoding kind of the content in LSP protocol.
    proto::PositionEncodingKind kind;
};

}  // namespace clice
