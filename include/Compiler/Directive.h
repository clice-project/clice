#pragma once

#include <Compiler/Clang.h>

namespace clice {

struct Include {
    /// The path of the included file.
    llvm::StringRef path;

    /// Location of the directive identifier.
    clang::SourceLocation loc;

    /// Range of the filename.
    clang::SourceRange range;
};

struct Condition {
    enum BranchKind : uint8_t {
        If = 0,
        Elif,
        Ifdef,
        Elifdef,
        Ifndef,
        Elifndef,
        Else,
        EndIf,
    };

    enum ConditionValue : uint8_t {
        True = 0,
        False,
        Skipped,
        None,
    };

    /// Kind of the branch.
    BranchKind kind;

    /// Value of the condition.
    ConditionValue value;

    /// Location of the directive identifier.
    clang::SourceLocation loc;

    /// Range of the condition.
    clang::SourceRange conditionRange;
};

struct MacroRef {
    enum Kind : uint8_t {
        Def = 0,
        Ref,
        Undef,
    };

    /// Kind of the macro reference.
    Kind kind;

    /// The location of the macro name.
    clang::SourceLocation loc;

    /// The macro definition information.
    const clang::MacroInfo* macro;
};

/// Do we need to store pragma information?
struct Pragma {};

struct Directive {
    std::vector<Include> includes;
    std::vector<Condition> conditions;
    std::vector<MacroRef> macros;

    /// Tell preprocessor to collect directives information and store them in `directives`.
    static void attach(clang::Preprocessor& pp,
                       llvm::DenseMap<clang::FileID, Directive>& directives);
};

}  // namespace clice
