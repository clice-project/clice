#pragma once

#include "Basic/Lifecycle.h"
#include "AST/SourceCode.h"
#include "clang/Basic/SourceLocation.h"

namespace clice {

/// A helper class to convert `Position, Range, Location` between 1-1 encoding based clang and 0-0
/// encoding based LSP. The conversion of DocumentUri is also supported.
class SourceConverter {
public:
    /// [(origin, new)],  map origin header directory to another source directory.
    using SourceDirMapping = std::vector<std::pair<std::string, std::string>>;

    /// Construct a `SourceConverter` with the specified encoding kind and empty source map.
    explicit SourceConverter(proto::PositionEncodingKind kind = proto::PositionEncodingKind::UTF8) :
        kind(kind), sourceMap() {}

    SourceConverter(proto::PositionEncodingKind kind, SourceDirMapping sourceMap) :
        kind(kind), sourceMap(std::move(sourceMap)) {}

    SourceConverter(const SourceConverter&) = delete;

    SourceConverter(SourceConverter&&) = default;

    /// Measure the length (character count) of the content with the specified encoding kind.
    std::size_t remeasure(llvm::StringRef content) const;

    /// Same as the below, but input is raw offset to the content beginning.
    proto::Position toPosition(llvm::StringRef content, std::uint32_t offset) const;

    /// Convert a clang::SourceLocation to a proto::Position according to the
    /// specified encoding kind. Note that `SourceLocation` in clang is 1-based and
    /// is always encoded in UTF-8.
    proto::Position toPosition(clang::SourceLocation location,
                               const clang::SourceManager& SM) const;

    /// Convert a clang::SourceRange to proto::Range according to the specified encoding kind.
    proto::Range toRange(clang::SourceRange range, const clang::SourceManager& SM) const;

    /// Same as the above, but input is a `LocalSourceRange` and the content is provided.
    proto::Range toRange(LocalSourceRange range, llvm::StringRef content) const;

    /// Convert a clang::SourceRange to LocalSourceRange.
    LocalSourceRange toLocalRange(clang::SourceRange range, const clang::SourceManager& SM) const;

    /// Convert a proto::Position to a file offset in the content with the specified
    /// encoding kind.
    std::uint32_t toOffset(llvm::StringRef content, proto::Position position) const;

    /// Get the encoding kind of the content in LSP protocol.
    proto::PositionEncodingKind encodingKind() const {
        return kind;
    }

    /// Convert a real path of a file to URI. Crash if failed.
    static proto::DocumentUri toURI(llvm::StringRef fspath);

    /// Convert a file URI to real path with `clice::fs::real_path`. Crash if failed.
    static std::string toPath(llvm::StringRef uri);

private:
    /// The encoding kind of the content in LSP protocol.
    proto::PositionEncodingKind kind;

    /// A user-defined map from header file to its source directory.
    SourceDirMapping sourceMap;
};

}  // namespace clice
