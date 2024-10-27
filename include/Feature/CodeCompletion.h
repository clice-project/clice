#pragma once

#include <Basic/Location.h>

namespace clice::proto {

struct CompletionClientCapabilities {};

enum CompletionItemKind {
    Text = 1,
    Method = 2,
    Function = 3,
    Constructor = 4,
    Field = 5,
    Variable = 6,
    Class = 7,
    Interface = 8,
    Module = 9,
    Property = 10,
    Unit = 11,
    Value = 12,
    Enum = 13,
    Keyword = 14,
    Snippet = 15,
    Color = 16,
    File = 17,
    Reference = 18,
    Folder = 19,
    EnumMember = 20,
    Constant = 21,
    Struct = 22,
    Event = 23,
    Operator = 24,
    TypeParameter,
};

enum CompletionItemTag {
    /// Render a completion as obsolete, usually using a strike-out.
    Deprecated = 1,
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
    std::vector<CompletionItemTag> tags;

    // FIXME:
    // ...
};

}  // namespace clice::proto

namespace clice {
class Compiler;
}

namespace clice::config {

struct CodeCompletionOption {
    // TODO:
};

}  // namespace clice::config

namespace clice::feature {

/// Run code completion in given file and location. `compiler` should be
/// set properly if any PCH or PCM is needed. Each completion requires a
/// new compiler instance.
std::vector<proto::CompletionItem> codeCompletion(Compiler& compiler,
                                                  llvm::StringRef filepath,
                                                  proto::Position position,
                                                  const config::CodeCompletionOption& option);

}  // namespace clice::feature
