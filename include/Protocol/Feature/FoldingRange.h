#pragma once

#include "../Basic.h"

namespace clice::proto {

struct FoldingRangeClientCapabilities {
    /// The maximum number of folding ranges that the client prefers to receive
    /// per document. The value serves as a hint, servers are free to follow the
    /// limit.
    optional<uinteger> rangeLimit;

    /// If set, the client signals that it only supports folding complete lines.
    /// If set, client will ignore specified `startCharacter` and `endCharacter`
    /// properties in a FoldingRange.
    bool lineFoldingOnly = false;

    /// Specific options for the folding range kind.
    struct {
        /// The folding range kind values the client supports. When this
        /// property exists the client also guarantees that it will
        /// handle values outside its set gracefully and falls back
        /// to a default value when unknown.
        array<string> valueSet;
    } foldingRangeKind;

    /// Specific options for the folding range.
    struct {
        /// If set, the client signals that it supports setting collapsedText on
        /// folding ranges to display custom labels instead of the default text.
        bool collapsedText = false;
    } foldingRange;
};

using FoldingRangeOptions = bool;

struct FoldingRangeParams {
    /// The text document.
    TextDocumentIdentifier textDocument;
};

using FoldingRangeKind = string;

struct FoldingRange {
    /// The zero-based start line of the range to fold. The folded area starts
    /// after the line's last character. To be valid, the end must be zero or
    /// larger and smaller than the number of lines in the document.
    uinteger startLine;

    /// The zero-based character offset from where the folded range starts. If
    /// not defined, defaults to the length of the start line.
    uinteger startCharacter;

    /// The zero-based end line of the range to fold. The folded area ends with
    /// the line's last character. To be valid, the end must be zero or larger
    /// and smaller than the number of lines in the document.
    uinteger endLine;

    /// The zero-based character offset before the folded range ends. If not
    /// defined, defaults to the length of the end line.
    uinteger endCharacter;

    /// Describes the kind of the folding range such as `comment` or `region`.
    /// The kind is used to categorize folding ranges and used by commands like
    /// 'Fold all comments'. See [FoldingRangeKind](#FoldingRangeKind) for an
    /// enumeration of standardized kinds.
    FoldingRangeKind kind;

    /// The text that the client should show when the specified range is
    /// collapsed. If not defined or not supported by the client, a default
    /// will be chosen by the client.
    string collapsedText;
};

}  // namespace clice::proto
