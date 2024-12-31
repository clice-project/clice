#pragma once

#include <Compiler/Clang.h>

namespace clice {

struct Include {
    /// Location of the `include` token.
    clang::SourceLocation include;

    /// The file id of the included file. If empty, it means the file
    /// is skipped because of header guard optimization.
    clang::FileID fid;

    /// The path of the included file.
    llvm::StringRef path;

    /// Range of the filename.
    clang::SourceRange range;

    bool isSkipped() const {
        return fid.isInvalid();
    }
};

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
    std::vector<Condition> conditions;
    std::vector<MacroRef> macros;

    /// Tell preprocessor to collect directives information and store them in `directives`.
    static void attach(clang::Preprocessor& pp,
                       llvm::DenseMap<clang::FileID, Directive>& directives);
};

}  // namespace clice
