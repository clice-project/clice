#pragma once

#include "Basic/Location.h"
#include "Support/Support.h"

namespace clice {

struct TranslationUnit;

struct SourceLocation {
    /// The line number (1-based).
    uint32_t line;

    /// The column number (1-based).
    uint32_t column;

    /// The file name.
    std::string filename;

    /// The include file of this location.
    uint32_t includeFile;
};

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
        uint32_t index;
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

    /// All include locations introduced by this translation unit.
    /// Note that if a file has guard macro or pragma once, we will
    /// record it at most once.
    std::vector<SourceLocation> locations;
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
