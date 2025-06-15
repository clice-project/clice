#pragma once

#include "Directive.h"

namespace clice {

/// This is a high-level wrapper for the Clang API, designed to shield users from its
/// complexity. We have encapsulated all the essential interfaces from Clang within this
///  class, each with detailed documentation.
class CompilationUnit {
public:
    /// The kind describes how we preprocess ths source file
    /// to get this compilation unit.
    enum class Kind : std::uint8_t {
        /// From preprocessing the source file. Therefore diretives
        /// are available but AST nodes are not.
        Preprocess,

        /// From indexing the static source file.
        Indexing,

        /// From building preamble for the source file.
        Preamble,

        /// From building precompiled module for the module interface unit.
        ModuleInterface,

        /// From building normal AST for source file, interested file and top level
        /// declarations are available.
        SyntaxOnly,

        /// From running code completion for the source file(preamble is applied).
        Completion,
    };

    using enum Kind;

    CompilationUnit(const CompilationUnit&) = delete;

    CompilationUnit(CompilationUnit&& other) : kind(other.kind), opaque(other.opaque) {
        other.opaque = nullptr;
    }

    ~CompilationUnit();

public:
    /// Given a macro location, return its top level spelling location(the location
    // of the token that the result token is expanded from, may from macro argument
    // or macro definition).
    clang::SourceLocation spelling_location(clang::SourceLocation location);

    /// Given a macro location, return its top level expansion location(the location of
    // macro expansion).
    clang::SourceLocation expansion_location(clang::SourceLocation location);

private:
    /// The kind of this compilation unit.
    Kind kind;

    /// The actual data hold all information.
    void* opaque;
};

}  // namespace clice
