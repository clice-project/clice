#pragma once

#include "Basic/Location.h"

namespace clice {

struct CompilationParams;

namespace config {

struct SignatureHelpOption {};

}  // namespace config

namespace feature {

struct SignatureHelpItem {};

using SignatureHelpResult = std::vector<SignatureHelpItem>;

SignatureHelpResult signatureHelp(CompilationParams& params,
                                  const config::SignatureHelpOption& option);

}  // namespace feature

}  // namespace clice

