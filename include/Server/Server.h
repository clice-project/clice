#pragma once

#include <Protocol/Protocol.h>
#include "Scheduler.h"
#include "Transport.h"

namespace clice {

// global server instance
extern class Server server;

/// core class responsible for starting the server
class Server {
    std::unique_ptr<Transport> transport;
    std::unique_ptr<Scheduler> scheduler;
    std::map<std::string_view, void (*)(std::string_view)> methods;

public:
    int run(int argc, char** argv);
    int exit();
    void handle_message(std::string_view message);

    template <auto ptr>
    void register_method(std::string_view name) {
        methods[name] = []<typename R, typename... Args>(R (*)(Args...)) {
            return [](std::string_view message) {
                // auto input = json::parse(message);
                // auto params = deserialize<Args>(input["params"]);
                // ptr(params);
            };
        }(ptr);
    }

private:
    /*===================================================/
    /                                                    /
    /==================== LSP methods ===================/
    /                                                    /
    /===================================================*/

    // Server lifecycle
    void initialize(const InitializeParams& params);

    // Text Document Synchronization
    Task<void> didOpen(const DidOpenTextDocumentParams& params);
    Task<void> didChange(const DidChangeTextDocumentParams& params);
    Task<void> didClose(const DidCloseTextDocumentParams& params);
    Task<void> didSave(const DidSaveTextDocumentParams& params);

    // Language Features
    Task<void> declaration();
    Task<void> definition();
    Task<void> typeDefinition();
    Task<void> implementation();
    Task<void> references();
    Task<void> callHierarchy();
    /* ... */
    Task<void> hover();
    Task<void> codeLens();
    /* ... */
    Task<void> foldingRange();
    Task<void> selectionRange();
    Task<void> documentSymbol();
    Task<void> semanticTokens();
    Task<void> inlineValue();
};

}  // namespace clice
