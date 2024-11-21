#include "Basic/URI.h"
#include "Server.h"
#include "Compiler/Compiler.h"

namespace clice {

namespace proto {

struct DidOpenTextDocumentParams {
    /// The document that was opened.
    TextDocumentItem textDocument;
};

}  // namespace proto

class TranslationUnit {
    using Callback = llvm::unique_function<void(Compiler&)>;

public:
    void addFile(llvm::StringRef uri, llvm::StringRef content) {
        /// 1. build PCH or PCM
        /// 2. build AST
    }

private:
    bool isBuilding = false;
    std::string path;
    std::unique_ptr<Compiler> compiler;
    llvm::SmallVector<Callback> callbacks;
};

class FileManager {
public:

private:
    llvm::StringMap<TranslationUnit> files;
};

class LSPServer : public Server {
public:
    LSPServer(const config::ServerOption& option) : Server(option) {
        addMethod("initialize", &LSPServer::onInitialize);
        addNotification("initialized", &LSPServer::onInitialized);
        addNotification("textDocument/didOpen", &LSPServer::onDidOpen);
    }

    template <typename Params, typename R>
    void addMethod(std::string_view method, R (LSPServer::*handler)(const Params&)) {
        methods[method] = [this, handler](json::Value params) {
            return json::serialize((this->*handler)(json::deserialize<Params>(params)));
        };
    }

    template <typename Params>
    void addNotification(std::string_view method, void (LSPServer::*handler)(const Params&)) {
        notifications[method] = [this, handler](json::Value params) {
            (this->*handler)(json::deserialize<Params>(params));
        };
    }

    void run() {
        auto loop = [&]() {
            if(hasMessage()) {
                auto& json = peek();
                if(json.kind() == json::Value::Object) {
                    auto object = json.getAsObject();
                    dispatch(std::move(json));
                } else {
                    llvm::errs() << "Strange message.\n";
                    consume();
                }
            }
        };
        Server::run(loop);
    }

    void dispatch(json::Value request);

public:
    auto onInitialize(const proto::InitializeParams& params) -> proto::InitializeResult;

    void onInitialized(const proto::InitializedParams& params);

    void onDidOpen(const proto::DidOpenTextDocumentParams& params);

private:
    llvm::StringMap<llvm::unique_function<json::Value(json::Value)>> methods;
    llvm::StringMap<llvm::unique_function<void(json::Value)>> notifications;
};

}  // namespace clice
