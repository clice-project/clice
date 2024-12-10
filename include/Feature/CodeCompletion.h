#pragma once

#include <Basic/Document.h>
#include <Support/JSON.h>

namespace clice {
struct CompliationParams;
}

namespace clice::proto {

struct CompletionClientCapabilities {};

struct CompletionOptions {
    std::vector<string> triggerCharacters;
};

struct CompletionItemKind : refl::Enum<CompletionItemKind> {
    enum Kind : uint8_t {
        Invalid = 0,
        Text,
        Method,
        Function,
        Constructor,
        Field,
        Variable,
        Class,
        Interface,
        Module,
        Property,
        Unit,
        Value,
        Enum,
        Keyword,
        Snippet,
        Color,
        File,
        Reference,
        Folder,
        EnumMember,
        Constant,
        Struct,
        Event,
        Operator,
        TypeParameter,
    };

    using Enum::Enum;

    constexpr inline static auto InvalidEnum = Invalid;
};

enum CompletionItemTag {
    /// Render a completion as obsolete, usually using a strike-out.
    Deprecated = 1,
};

struct InsertReplaceEdit {
    /// The string to be inserted.
    string newText;

    /// The range if the insert is requested.
    Range insert;

    /// The range if the replace is requested.
    Range replace;
};

struct CompletionItem {
    /// The label of this completion item.
    /// If label details are provided the label itself should
    /// be an unqualified name of the completion item.
    string label;

    /// FIXME:
    /// labelDetails?: CompletionItemLabelDetails;

    // The kind of this completion item. Based of the kind
    // an icon is chosen by the editor. The standardized set
    // of available values is defined in `CompletionItemKind`.
    CompletionItemKind kind;

    /// Tags for this completion item.
    /// std::vector<CompletionItemTag> tags;

    /// A human-readable string with additional information
    /// about this item, like type or symbol information.
    string detail;

    /// A human-readable string that represents a doc-comment.
    /// string documentation;

    /// A string that should be used when comparing this item
    /// with other items. When omitted the label is used
    /// as the sort text for this item.
    /// string sortText;

    TextEdit textEdit;
};

using CompletionParams = TextDocumentPositionParams;

using CompletionResult = std::vector<CompletionItem>;

}  // namespace clice::proto

namespace clice::config {

struct CodeCompletionOption {};

}  // namespace clice::config

namespace clice::feature {

json::Value capability(json::Value clientCapabilities);

/// Run code completion in given file and location. `compiler` should be
/// set properly if any PCH or PCM is needed. Each completion requires a
/// new compiler instance.
proto::CompletionResult codeCompletion(CompliationParams& compliation,
                                       uint32_t line,
                                       uint32_t column,
                                       llvm::StringRef file,
                                       const config::CodeCompletionOption& option);

}  // namespace clice::feature
