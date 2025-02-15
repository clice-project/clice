#pragma once

#include "Basic/Location.h"

namespace clice::proto {

struct SignatureInformation {
    /// The label of this signature. Will be shown in the UI.
    string label;

    // FIXME:
    // ...
};

struct SignatureHelp {
    /// One or more signatures. If no signatures are available the signature help
    /// request should return `null`.
    std::vector<SignatureInformation> signatures;
};

struct SignatureHelpParams {};

}  // namespace clice::proto

namespace clice {
class Compiler;
}

namespace clice::config {

struct SignatureHelpOption {};

}  // namespace clice::config

namespace clice::feature {

proto::SignatureHelp signatureHelp(Compiler& compiler,
                                   llvm::StringRef filepath,
                                   proto::Position position,
                                   const config::SignatureHelpOption& option);

}
