#pragma once

#include <vector>

#include "Shared.h"
#include "Feature/SemanticToken.h"
#include "Feature/FoldingRange.h"
#include "Feature/DocumentLink.h"
#include "Feature/DocumentSymbol.h"

#include "llvm/ADT/DenseMap.h"
#include "clang/Basic/SourceLocation.h"

namespace clice::index {

class FeatureIndex {
public:
    FeatureIndex(char* base, std::size_t size) : base(base), size(size) {}

    /// The path of source file.
    llvm::StringRef path();

    /// The content of source file.
    llvm::StringRef content();

    feature::SemanticTokens semanticTokens() const;

    feature::FoldingRanges foldingRanges() const;

    feature::DocumentLinks documentLinks() const;

    feature::DocumentSymbols documentSymbols() const;

    static Shared<std::vector<char>> build(CompilationUnit& unit);

public:
    char* base;
    std::size_t size;
};

}  // namespace clice::index
