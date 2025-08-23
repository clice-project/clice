#pragma once

#include <vector>
#include <cstdint>

#include "llvm/ADT/StringRef.h"
#include "Protocol/Feature/SignatureHelp.h"

namespace clice {

struct CompilationParams;

namespace config {

struct SignatureHelpOption {};

}  // namespace config

namespace feature {

proto::SignatureHelp signature_help(CompilationParams& params,
                                    const config::SignatureHelpOption& option);

}  // namespace feature

}  // namespace clice
