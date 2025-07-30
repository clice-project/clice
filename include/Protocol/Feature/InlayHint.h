#pragma once

#include "../Basic.h"

namespace clice::proto {

struct InlayHintClientCapabilities {};

struct InlayHintOptions {
    /// The server provides support to resolve additional
    /// information for an inlay hint item.
    bool resolveProvider;
};

}  // namespace clice::proto
