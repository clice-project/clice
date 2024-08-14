#include <Clang/Clang.h>

namespace clice {

// note that `#` is not part of the directive token. it is a separate token.
class Directive {
    friend class DirectiveCollector;

private:
    /// represent a `#include` directive.
    struct Header {
        SourceLocation hashLoc;
        Token includeTok;
        StringRef fileName;
        bool isAngled;
        clang::CharSourceRange filenameRange;
        clang::OptionalFileEntryRef file;
        StringRef searchPath;
        StringRef relativePath;
        const clang::Module* suggestedModule;
        bool moduleImported;
        clang::SrcMgr::CharacteristicKind fileType;
    };

    /// represent a `#define` directive.
    struct Define {
        Token name;
        const clang::MacroDirective* directive;
    };

    /// represent a `#undef` directive.
    struct Undef {
        Token name;
        const clang::MacroDefinition* definition;
        const clang::MacroDirective* directive;
    };

    /// represent a `#pragma` directive.
    struct Pragma {
        /// the location of `pragma` directive.
        SourceLocation location;
        /// the kind of the pragma directive.
        clang::PragmaIntroducerKind kind;
    };

    /// represent a condition in `#if` or `#elif` directive.
    struct Condition {
        /// the source range of the condition.
        SourceRange range;
        /// the evaluated value of the condition.
        PPCallbacks::ConditionValueKind value;
    };

    /// represent a `#if` directive
    struct If {
        /// the location of `if` directive.
        SourceLocation location;
        Condition condition;
    };

    /// represent a `#elif` directive
    struct Elif {
        /// the location of `elif` directive.
        SourceLocation location;
        Condition condition;
    };

    /// represent a macro condition in `#ifdef` or `#elifdef` directive.
    struct MacroCondition {
        /// the source range of the macro name.
        Token token;
        /// the corresponding macro definition(may be null).
        clang::MacroDefinition definition;
    };

    /// represent a `#ifdef` directive
    struct Ifdef {
        /// the location of `ifdef` directive.
        SourceLocation location;
        MacroCondition condition;
    };

    /// represent a `#elifdef` directive
    struct Elifdef {
        /// the location of `elifdef` directive.
        SourceLocation location;
        MacroCondition condition;
    };

    struct Else {
        /// the location of `else` directive.
        SourceLocation location;
    };

    struct Endif {
        /// the location of `endif` directive.
        SourceLocation location;
    };

    struct IfBlock {
        If if_;
        std::vector<Elif> elifs;
        std::optional<Else> else_;
        Endif endif;
    };

    struct IfdefBlock {
        Ifdef ifdef;
        std::vector<Elifdef> elifdefs;
        std::optional<Else> else_;
        Endif endif;
    };

private:
    std::vector<Header> headers;
    std::vector<Define> defines;
    std::vector<Undef> undefs;
    std::vector<Pragma> pragmas;
    std::vector<IfBlock> ifBlocks;
    std::vector<IfdefBlock> ifdefBlocks;

public:
    Directive(clang::Preprocessor& pp);

    auto& Headers() { return headers; }

    auto& Defines() { return defines; }

    auto& Undefs() { return undefs; }

    auto& Pragmas() { return pragmas; }

    auto& IfBlocks() { return ifBlocks; }

    auto& IfdefBlocks() { return ifdefBlocks; }
};

#if 1111

#endif

}  // namespace clice
