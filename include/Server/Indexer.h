#pragma once

#include "Basic/Location.h"
#include "Support/Support.h"

namespace clice {

struct TranslationUnit;

/// Describes the context of a header file to uniquely identify its AST.
/// A header file may generate different ASTs depending on the inclusion context.
/// Even within the same source file, the AST may vary at different locations due
/// to preprocessor directives, macro definitions, or other compilation settings.
using IncludeChain = std::vector<SourceLocation>;
using IncludeChainRef = llvm::ArrayRef<SourceLocation>;

struct Header {
    /// The path of the header file.
    std::string srcPath;

    struct Context {
        /// The index file path(not include suffix, e.g. `.sidx` and `.fidx`).
        std::string indexPath;

        /// The include chain that introduces this context.
        IncludeChain chain;
    };

    /// All header contexts.
    llvm::DenseMap<TranslationUnit*, Context> contexts;
};

struct TranslationUnit {
    /// The source file path.
    std::string srcPath;

    /// The index file path(not include suffix, e.g. `.sidx` and `.fidx`).
    std::string indexPath;

    /// All headers included by this translation unit.
    std::vector<Header*> headers;

    /// All include chains are introduced by this translation unit.
    std::vector<IncludeChain> chains;
};

/// Responsible for index all files, distinguish active and inactive files.
class Indexer {
public:
    /// Index the given file path.
    void index(llvm::StringRef path);

    void index(llvm::StringRef file, class ASTInfo& info);

    llvm::json::Value toJSON() const;

    void load();

    void save();

private:
    llvm::StringMap<Header*> headers;

    llvm::StringMap<TranslationUnit*> translationUnits;
};

}  // namespace clice
