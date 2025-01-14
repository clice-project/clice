#pragma once

#include "Basic.h"

namespace clice::proto {

struct WorkspaceFolder {
    /// The associated URI for this workspace folder.
    URI uri;

    /// The name of the workspace folder. Used to refer to this workspace folder
    /// in the user interface.
    std::string name;
};

struct DidChangeWatchedFilesParams {};

}  // namespace clice::proto
