#pragma once

#include "Basic.h"

namespace clice::proto {

struct WorkspaceFolder {
    /// The associated URI for this workspace folder.
    URI uri;

    /// The name of the workspace folder. Used to refer to this
    /// workspace folder in the user interface.
    string name;
};

struct WorkspaceClientCapabilities {};

struct WorkspaceSymbolOptions {};

struct WorkspaceFoldersServerCapabilities {
    /// The server has support for workspace folders.
    bool supported = true;
};

struct WorkspaceServerCapabilities {
    /// The server supports workspace folder.
    WorkspaceFoldersServerCapabilities workspaceFolders;
};

}  // namespace clice::proto
