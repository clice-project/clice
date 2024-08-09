#pragma once

#include "../Basic.h"

namespace clice::protocol {

enum class SemanticTokenType : uint8_t {
    /// Represents a comment.
    Comment,
    /// Represents a number literal.
    Number,
    /// Represents a character literal.
    Char,
    /// Represents a string literal.
    String,
    /// Represents a C/C++ keyword (e.g., `int`, `class`, `struct`).
    Keyword,
    /// Represents a compiler built-in macro, function, or keyword (e.g., `__stdcall`,
    /// `__attribute__`, `__FUNCSIG__`).
    Builtin,
    /// Represents a preprocessor directive (e.g., `#include`, `#define`, `#if`).
    Directive,
    /// Represents a header file path (e.g., `<iostream>`).
    HeaderPath,
    /// Represents a C/C++ macro name, both in definition and invocation.
    Macro,
    /// Represents a C/C++ macro parameter, both in definition and invocation.
    MacroParameter,
    /// Represents a C++ namespace name.
    Namespace,
    /// Represents a C/C++ type name.
    Type,
    /// Represents a C/C++ struct name.
    Struct,
    /// Represents a C/C++ union name.
    Union,
    /// Represents a C/C++ class name.
    Class,
    /// Represents a C/C++ field name.
    Field,
    /// Represents a C/C++ enum name.
    Enum,
    /// Represents a C/C++ enum field (member) name.
    EnumMember,
    /// Represents a C/C++ variable name.
    Variable,
    /// Represents a C/C++ function name.
    Function,
    /// Represents a C++ method name.
    Method,
    /// Represents a C/C++ function/method parameter name.
    Parameter,
    /// Represents a C++ dependent name in a template context (e.g., `name` in `auto y = T::name`
    /// where T is a template parameter).
    /// Note: This includes `T::template name(...)`; it's not possible to distinguish whether a name
    /// is a function or a functor in a dependent context.
    DependentName,
    /// Represents a C++ dependent type name (e.g., `type` in `typename std::vector<T>::type` where
    /// T is a template parameter).
    /// Note: This includes `typename T::template type<...>`.
    DependentType,
    /// Represents a C++20 concept name.
    Concept,
    /// Represents a C++11 attribute name.
    Attribute,
    /// Represents parentheses `()`.
    Paren,
    /// Represents curly braces `{}`.
    Brace,
    /// Represents square brackets `[]`.
    Bracket,
    /// Represents angle brackets `<>`.
    Angle,
    /// Represents the scope resolution operator `::`.
    Scope,
    /// Represents built-in operators (e.g., `+` in `1 + 2`).
    Operator,
    /// Represents punctuation in non-expression contexts (e.g., `;`, `,` in enum declaration, `=`
    /// and `&` in lambda capture).
    Delimiter,
    Unknown,
    Invalid
};

enum class SemanticTokenModifier : uint32_t {
    /// emit for a name in declaration.
    /// e.g. function declaration, variable declaration, class declaration.
    Declaration,
    /// emit for a name in definition.
    /// e.g. function definition, variable definition, class definition.
    Definition,
    /// emit for a name in reference(not declaration or definition).
    /// e.g. `x` in `x + 1`, `X` in `X::type`
    Reference,
    Const,
    Constexpr,
    Consteval,
    Virtual,
    PureVirtual,
    Inline,
    Static,
    Deprecated,
    Local,
    /// emit for left bracket.
    Left,
    /// emit for right bracket.
    Right,
    /// emit for operators which are part of type.
    /// e.g. `*` in `int*`, `&` in `int&`.
    Intype,
    /// emit for operators which are overloaded.
    /// e.g. `+` in `std::string("123") + c;`
    Overloaded,
    None,
};

/// Client Capability:
/// - property name(optional): `textDocument.semanticTokens`
/// - property type: `SemanticTokensClientCapabilities` defined as follows:
struct SemanticTokensClientCapabilities {
    /// Whether implementation supports dynamic registration. If this is set to `true` the client
    bool dynamicRegistration = false;

    struct Requests {
        // FIXME:
    };

    /// The token types that the client supports.
    std::vector<String> tokenTypes;

    /// The token modifiers that the client supports.
    std::vector<String> tokenModifiers;

    /// The formats the client supports.
    /// formats: TokenFormat[];

    /// Whether the client supports tokens that can overlap each other.
    bool overlappingTokenSupport = false;

    /// Whether the client supports tokens that can span multiple lines.
    bool multilineTokenSupport = false;

    /// Whether the client allows the server to actively cancel a semantic token request.
    bool serverCancelSupport = false;

    /// Whether the client uses semantic tokens to augment existing syntax tokens.
    bool serverCancelSupports = false;
};

struct SemanticTokensLegend {
    /// The token types a server uses.
    std::vector<String> tokenTypes;

    /// The token modifiers a server uses.
    std::vector<String> tokenModifiers;
};

/// Server Capability:
/// - property name(optional): `textDocument.semanticTokens`
/// - property type: `SemanticTokensOptionss` defined as follows:
struct SemanticTokensOptions {
    /// The legend used by the server.
    SemanticTokensLegend legend;

    /// Server supports providing semantic tokens for a specific range.
    bool range = false;

    /// Server supports providing semantic tokens for a full document.
    bool full = false;
};

/// Request:
/// - method: `textDocument/semanticTokens/full`
/// - params: `SemanticTokensParams` defined as follows:
struct SemanticTokensParamsBody {
    /// The text document.
    TextDocumentIdentifier textDocument;
};

using SemanticTokensParams = Combine<
    // WorkDoneProgressParams,
    // PartialResultParams,
    SemanticTokensParamsBody>;

/// Response:
/// - result: `SemanticTokens` defined as follows:
struct SemanticTokens {
    /// An optional result id.
    String resultId;

    /// The actual tokens.
    std::vector<Integer> data;
};

}  // namespace clice::protocol
