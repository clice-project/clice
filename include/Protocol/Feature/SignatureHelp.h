#pragma once

#include "../Basic.h"

namespace clice::proto {

struct SignatureHelpClientCapabilities {
    /**
     * The client supports the `activeParameter` property on
     * `SignatureInformation` literal.
     *
     * @since 3.16.0
     */
};

struct SignatureHelpOptions {
    /// The characters that trigger signature help automatically.
    array<string> triggerCharacters;

    /// List of characters that re-trigger signature help.
    ///
    /// These trigger characters are only active when signature help is already
    /// showing. All trigger characters are also counted as re-trigger
    /// characters.
    array<string> retriggerCharacters;
};

using SignatureHelpParams = TextDocumentPositionParams;

struct ParameterInformation {
    std::array<uinteger, 2> label;
};

struct SignatureInformation {
    string label;

    MarkupContent document;

    array<ParameterInformation> parameters;

    uinteger activeParameter;
};

struct SignatureHelp {
    array<SignatureInformation> signatures;

    uinteger activeSignature;
};

}  // namespace clice::proto
