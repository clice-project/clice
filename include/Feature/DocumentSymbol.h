#pragma once

#include "Basic/Document.h"
#include "AST/SourceCode.h"
#include "Index/Shared.h"
#include "Support/JSON.h"

namespace clice {

namespace proto {

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_documentSymbol

struct DocumentSymbolClientCapabilities {};

/// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#documentSymbolParams
struct DocumentSymbolParams {
    /// The text document.
    TextDocumentIdentifier textDocument;
};

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#symbolKind
struct SymbolKind : refl::Enum<SymbolKind> {
    enum Kind : uint8_t {
        Invalid = 0,
        File = 1,
        Module = 2,
        Namespace = 3,
        Package = 4,
        Class = 5,
        Method = 6,
        Property = 7,
        Field = 8,
        Constructor = 9,
        Enum = 10,
        Interface = 11,
        Function = 12,
        Variable = 13,
        Constant = 14,
        String = 15,
        Number = 16,
        Boolean = 17,
        Array = 18,
        Object = 19,
        Key = 20,
        Null = 21,
        EnumMember = 22,
        Struct = 23,
        Event = 24,
        Operator = 25,
        TypeParameter = 26,
    };

    using Enum::Enum;

    constexpr static auto InvalidEnum = Invalid;
};

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#symbolTag
struct SymbolTag : refl::Enum<SymbolTag> {
    enum Tag : uint8_t {
        Invalid = 0,
        Deprecated = 1,
    };

    using Enum::Enum;

    constexpr static auto InvalidEnum = Invalid;
};

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#documentSymbol
struct DocumentSymbol {
    /// The name of this symbol.
    string name;

    /// More detail for this symbol, e.g the signature of a function.
    string detail;

    /// The kind of this symbol.
    SymbolKind kind;

    /// Tags for this symbol.
    std::vector<SymbolTag> tags;

    /// The range enclosing this symbol not including leading/trailing whitespace but everything
    /// else.This information is typically used to determine if the clients cursor is inside the
    /// symbol to reveal in the symbol in the UI.
    Range range;

    /// The range that should be selected and revealed when this symbol is being picked, e.g. the
    /// name of a function. Must be contained by the `range`.
    Range selectionRange;

    /// Children of this symbol, e.g. properties of a class.
    std::vector<DocumentSymbol> children;
};

using DocumentSymbolResult = std::vector<DocumentSymbol>;

}  // namespace proto

class ASTInfo;
class SourceConverter;

namespace feature::document_symbol {

json::Value capability(json::Value clientCapabilities);

struct DocumentSymbol {
    /// The kind of this symbol.
    proto::SymbolKind kind;

    /// The name of this symbol.
    std::string name;

    /// More detail for this symbol, e.g the signature of a function.
    std::string detail;

    /// Tags for this symbol.
    std::vector<proto::SymbolTag> tags;

    /// Children of this symbol, e.g. properties of a class.
    std::vector<DocumentSymbol> children;

    /// The range enclosing the symbol not including leading/trailing whitespace but everything
    /// else.
    LocalSourceRange range;

    /// Must be contained by the `range`.
    LocalSourceRange selectionRange;
};

using Result = std::vector<DocumentSymbol>;

/// Get all document symbols in each file.
index::Shared<Result> documentSymbol(ASTInfo& AST);

struct MainFileOnlyFlag {};

/// Get document symbols in the main file.
Result documentSymbol(ASTInfo& AST, MainFileOnlyFlag _mainFileOnlyFlag);

/// Convert the result to LSP format.
proto::DocumentSymbol toLspType(const DocumentSymbol& result,
                                const SourceConverter& SC,
                                llvm::StringRef content);

/// Convert an array of document symbols to LSP format.
proto::DocumentSymbolResult toLspType(llvm::ArrayRef<DocumentSymbol> result,
                                      const SourceConverter& SC,
                                      llvm::StringRef content);

}  // namespace feature::document_symbol

}  // namespace clice
