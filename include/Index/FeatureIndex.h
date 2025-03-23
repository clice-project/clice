#pragma once

#include <vector>

#include "Shared.h"
#include "Feature/SemanticTokens.h"
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

    std::vector<feature::SemanticToken> semanticTokens() const;

    std::vector<feature::FoldingRange> foldingRanges() const;

    std::vector<feature::DocumentLink> documentLinks() const;

    std::vector<feature::DocumentSymbol> documentSymbols() const;

    static Shared<std::vector<char>> build(ASTInfo& AST);

public:
    char* base;
    std::size_t size;
};

}  // namespace clice::index
