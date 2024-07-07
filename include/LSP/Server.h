#pragma once

#include <uv.h>
#include <string_view>

#include "Protocol.h"

namespace clice {

// global server instance
extern class Server server;

/// core class responsible for starting the server
class Server {
    uv_loop_t* loop;
    uv_pipe_t stdin_pipe;
    uv_pipe_t stdout_pipe;

public:
    int start();
    int exit();
    void handle_message(std::string_view message);

    /*===================================================/
    /                                                    /
    /==================== LSP methods ===================/
    /                                                    /
    /===================================================*/

    // Server lifecycle
    void initialize(InitializeParams& params);

    // Text Document Synchronization

    // Language Features
    void declaration();
    void definition();
    void typeDefinition();
    void implementation();
    void references();
    void callHierarchy();
    /* ... */
    void hover();
    void codeLens();
    /* ... */
    void foldingRange();
    void selectionRange();
    void documentSymbol();
    void semanticTokens();
    void inlineValue();
};

}  // namespace clice
