#pragma once

#include "Clang.h"

namespace clice {

/// Information about `#include` directive.
struct Include {
    /// The file id of the included file. If the file is skipped because of
    /// include guard, or `#pragma once`, this will be invalid.
    clang::FileID fid;

    /// Location of the `include`.
    clang::SourceLocation location;
};

/// Information about `__has_include` directive.
struct HasInclude {
    /// The path of the included file.
    llvm::StringRef path;

    /// Location of the filename token start.
    clang::SourceLocation location;
};

/// Information about `#if`, `#ifdef`, `#ifndef`, `#elif`,
/// `#elifdef`, `#else`, `#endif` directive.
struct Condition {
    enum class BranchKind : uint8_t {
        If = 0,
        Elif,
        Ifdef,
        Elifdef,
        Ifndef,
        Elifndef,
        Else,
        EndIf,
    };

    using enum BranchKind;

    enum class ConditionValue : uint8_t {
        True = 0,
        False,
        Skipped,
        None,
    };

    using enum ConditionValue;

    /// Kind of the branch.
    BranchKind kind;

    /// Value of the condition.
    ConditionValue value;

    /// Location of the directive identifier.
    clang::SourceLocation loc;

    /// Range of the condition.
    clang::SourceRange conditionRange;
};

/// Information about macro definition, reference and undef.
struct MacroRef {
    enum class Kind : uint8_t {
        Def = 0,
        Ref,
        Undef,
    };

    using enum Kind;

    /// The macro definition information.
    const clang::MacroInfo* macro;

    /// Kind of the macro reference.
    Kind kind;

    /// The location of the macro name.
    clang::SourceLocation loc;
};

/// Do we need to store pragma information?
struct Pragma {};

struct Directive {
    std::vector<Include> includes;
    std::vector<HasInclude> hasIncludes;
    std::vector<Condition> conditions;
    std::vector<MacroRef> macros;
    std::vector<Pragma> pragmas;

    /// Tell preprocessor to collect directives information and store them in `directives`.
    static void attach(clang::Preprocessor& pp,
                       llvm::DenseMap<clang::FileID, Directive>& directives);
};

}  // namespace clice
