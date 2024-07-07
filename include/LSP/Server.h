#pragma once

#include "Protocol.h"
#include "Transport.h"

namespace clice {

// global server instance
extern class Server server;

/// core class responsible for starting the server
class Server {
    uv_loop_t* loop;
    std::unique_ptr<Transport> transport;

public:
    int run();
    int exit();
    void handle_message(std::string_view message);

    /*===================================================/
    /                                                    /
    /==================== LSP methods ===================/
    /                                                    /
    /===================================================*/

    // Server lifecycle
    void initialize(const InitializeParams& params);

    // Text Document Synchronization
    void didOpen(this Server& self, const DidOpenTextDocumentParams& params);
    void didChange(this Server& self, const DidChangeTextDocumentParams& params);
    void didClose(this Server& self, const DidCloseTextDocumentParams& params);
    void didSave(this Server& self, const DidSaveTextDocumentParams& params);

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
