#pragma once

#include "../Basic.h"

namespace clice::protocol {

enum class SemanticTokenTypes : uint8_t {
    Namespace = 0,
    Type,
    Class,
    Enum,
    Interface,
    Struct,
    TypeParameter,
    Parameter,
    Variable,
    Property,
    EnumMember,
    Event,
    Function,
    Method,
    Macro,
    Keyword,
    Modifier,
    Comment,
    String,
    Number,
    Regexp,
    Operator,
    Decorator
};

enum class SemanticTokenModifiers : uint8_t {
    Declaration = 0,
    Definition,
    Readonly,
    Static,
    Deprecated,
    Abstract,
    Async,
    Modification,
    Documentation,
    DefaultLibrary
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
