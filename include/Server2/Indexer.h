#pragma once

#include "Support/Support.h"

namespace clice {

struct TranslationUnit;

struct SourceLocation {
    /// The line number (1-based).
    uint32_t line;

    /// The include file of this location.
    uint32_t include;

    /// The file name.
    std::string filename;
};

/// Responsible for index all files, distinguish active and inactive files.
class Indexer {
public:

private:
    struct Header {
        /// The path of the header file.
        std::string srcPath;

        struct Context {
            /// The index file path(not include suffix, e.g. `.sidx` and `.fidx`).
            std::string path;

            /// The include chain that introduces this context.
            uint32_t include;
        };

        /// All header contexts.
        llvm::DenseMap<TranslationUnit*, std::vector<Context>> contexts;
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

    llvm::StringMap<Header*> headers;

    llvm::StringMap<TranslationUnit*> tus;
};

}  // namespace clice
