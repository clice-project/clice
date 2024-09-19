#include "Diagnostic.h"

namespace clice {

/// Represents a full conditional preprocessing block, from #if to #endif.
struct IfBlock {
    /// Represents a single conditional branch (e.g., #if, #ifdef, #elif).
    struct Branch {
        /// Location of the directive.
        /// NOTE: The `#` is a separate token and not necessarily adjacent.
        clang::SourceLocation location;

        /// Location of the condition expression.
        clang::SourceLocation condition;

        /// Evaluated value of the condition.
        clang::PPCallbacks::ConditionValueKind value;
    };

    /// Initial #if, #ifdef, or #ifndef directive.
    Branch if_;

    /// #elif, #elifdef, or #elifndef directives.
    std::vector<Branch> elifs;

    /// Location of the #else directive, if present.
    clang::SourceLocation elseLoc;

    /// Location of the #endif directive.
    clang::SourceLocation endifLoc;
};

struct Directive {
    /// map from the location of if/ifdef/ifndef to the corresponding if block.
    llvm::DenseMap<clang::SourceLocation, IfBlock> ifBlocks;
};

struct Directives {
    clang::SourceManager& sourceManager;
    llvm::DenseMap<clang::FileID, Directive> x;

    clang::CommentHandler* handler();
    std::unique_ptr<clang::PPCallbacks> callback();
};

}  // namespace clice
