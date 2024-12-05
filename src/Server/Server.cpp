#include "Server/Server.h"
#include "llvm/Support/CommandLine.h"

namespace clice {

namespace cl {

llvm::cl::opt<std::string> config("config",
                                  llvm::cl::desc("The path of the config file"),
                                  llvm::cl::value_desc("path"));

}  // namespace cl

int Server::run(int argc, const char** argv) {
    llvm::cl::SetVersionPrinter([](llvm::raw_ostream& os) { os << "clice version: 0.0.1\n"; });
    llvm::cl::ParseCommandLineOptions(argc, argv, "clice language server");

    if(cl::config.empty()) {
        log::warn("No config file specified; using default configuration.");
    } else {
        config::parse(argv[0], cl::config.getValue());
        log::info("Successfully loaded configuration file from {0}.", cl::config.getValue());
    }

    auto dispatch = [this](json::Value value) -> async::promise<void> {
        assert(value.kind() == json::Value::Object);
        auto object = value.getAsObject();
        assert(object && "value is not an object");
        if(auto method = object->get("method")) {
            auto name = *method->getAsString();
            auto params = object->get("params");
            if(auto id = object->get("id")) {
                if(auto iter = requests.find(name); iter != requests.end()) {
                    log::info("Request: {0}", name.str());
                    co_await iter->second(std::move(*id),
                                          params ? std::move(*params) : json::Value(nullptr));
                } else {
                    log::warn("Unknown request: {0}", name.str());
                }
            } else {
                if(auto iter = notifications.find(name); iter != notifications.end()) {
                    log::info("Notification: {0}", name.str());
                    co_await iter->second(params ? std::move(*params) : json::Value(nullptr));
                } else {
                    log::warn("Unknown notification: {0}", name.str());
                }
            }
        }

        co_return;
    };

    async::start_server(dispatch, "127.0.0.1", 50051);

    return 0;
}

Server::Server() {
    addMethod("initialize", &Server::onInitialize);
    addMethod("initialized", &Server::onInitialized);
    addMethod("shutdown", &Server::onShutdown);
    addMethod("exit", &Server::onExit);

    addMethod("textDocument/didOpen", &Server::onDidOpen);
    addMethod("textDocument/didChange", &Server::onDidChange);
    addMethod("textDocument/didSave", &Server::onDidSave);
    addMethod("textDocument/didClose", &Server::onDidClose);

    addMethod("textDocument/declaration", &Server::onGotoDeclaration);
    addMethod("textDocument/definition", &Server::onGotoDefinition);
    addMethod("textDocument/typeDefinition", &Server::onGotoTypeDefinition);
    addMethod("textDocument/implementation", &Server::onGotoImplementation);
    addMethod("textDocument/references", &Server::onFindReferences);
    addMethod("textDocument/callHierarchy/prepare", &Server::onPrepareCallHierarchy);
    addMethod("textDocument/callHierarchy/incomingCalls", &Server::onIncomingCall);
    addMethod("textDocument/callHierarchy/outgoingCalls", &Server::onOutgoingCall);
    addMethod("textDocument/typeHierarchy/prepare", &Server::onPrepareTypeHierarchy);
    addMethod("textDocument/typeHierarchy/supertypes", &Server::onSupertypes);
    addMethod("textDocument/typeHierarchy/subtypes", &Server::onSubtypes);
    addMethod("textDocument/documentHighlight", &Server::onDocumentHighlight);
    addMethod("textDocument/documentLink", &Server::onDocumentLink);
    addMethod("textDocument/hover", &Server::onHover);
    addMethod("textDocument/codeLens", &Server::onCodeLens);
    addMethod("textDocument/foldingRange", &Server::onFoldingRange);
    addMethod("textDocument/documentSymbol", &Server::onDocumentSymbol);
    addMethod("textDocument/semanticTokens/full", &Server::onSemanticTokens);
    addMethod("textDocument/inlayHint", &Server::onInlayHint);
    addMethod("textDocument/completion", &Server::onCodeCompletion);
    addMethod("textDocument/signatureHelp", &Server::onSignatureHelp);
    addMethod("textDocument/codeAction", &Server::onCodeAction);
    addMethod("textDocument/formatting", &Server::onFormatting);
    addMethod("textDocument/rangeFormatting", &Server::onRangeFormatting);

    addMethod("context/current", &Server::onContextCurrent);
    addMethod("context/switch", &Server::onContextSwitch);
    addMethod("context/all", &Server::onContextAll);
}

}  // namespace clice
