#pragma once

#include "../Basic.h"

namespace clice::proto {

struct CompletionClientCapabilities {};

struct CompletionOptions {
    /// The additional characters, beyond the defaults provided by the client (typically
    /// [a-zA-Z]), that should automatically trigger a completion request. For example
    ///`.` in JavaScript represents the beginning of an object property or method and is
    /// thus a good candidate for triggering a completion request.
    //
    /// Most tools trigger a completion request automatically without explicitly
    /// requesting it using a keyboard shortcut (e.g. Ctrl+Space). Typically they
    /// do so when the user starts to type an identifier. For example if the user
    /// types `c` in a JavaScript file code complete will automatically pop up
    /// present `console` besides others as a completion item. Characters that
    /// make up identifiers don't need to be listed here.
    array<string> triggerCharacters;

    /// The server provides support to resolve additional information for a completion item.
    bool resolveProvider;

    struct CompletionItemCapabilities {
        /// The server has support for completion item label
        /// details (see also `CompletionItemLabelDetails`) when receiving
        /// a completion item in a resolve call.
        bool labelDetailsSupport;
    };

    /// The server supports the following `CompletionItem` specific capabilities.
    CompletionItemCapabilities completionItem;
};

using CompletionParams = TextDocumentPositionParams;

}  // namespace clice::proto
