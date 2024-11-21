#pragma once

#include "Server/Config.h"
#include "Basic/Document.h"
#include "Support/JSON.h"
#include "llvm/ADT/FunctionExtras.h"
#include "Async.h"
#include "Compiler/Compiler.h"

namespace clice::proto {

enum class TextDocumentSyncKind {
    None = 0,
    Full = 1,
    Incremental = 2,
};

struct ClientInfo {
    /// The name of the client as defined by the client.
    string name;
    /// The client's version as defined by the client.
    string version;
};

struct ClientCapabilities {};

struct Workplace {
    /// The associated URI for this workspace folder.
    string uri;

    /// The name of the workspace folder. Used to refer to this
    /// workspace folder in the user interface.
    string name;
};

struct InitializeParams {
    /// Information about the client
    ClientInfo clientInfo;

    /// The locale the client is currently showing the user interface
    /// in. This must not necessarily be the locale of the operating
    /// system.
    ///
    /// Uses IETF language tags as the value's syntax.
    /// (See https://en.wikipedia.org/wiki/IETF_language_tag)
    string locale;

    /// The capabilities provided by the client (editor or tool).
    ClientCapabilities capabilities;

    /// The workspace folders configured in the client when the server starts.
    /// This property is only available if the client supports workspace folders.
    /// It can be `null` if the client supports workspace folders but none are
    /// configured.
    std::vector<Workplace> workspaceFolders;
};

struct ServerCapabilities {
    std::string_view positionEncoding = "utf-16";
    TextDocumentSyncKind textDocumentSync = TextDocumentSyncKind::Full;
    // SemanticTokensOptions semanticTokensProvider;
};

struct InitializeResult {
    ServerCapabilities capabilities;

    struct {
        std::string_view name = "clice";
        std::string_view version = "0.0.1";
    } serverInfo;
};

struct InitializedParams {};

struct DidOpenTextDocumentParams {
    /// The document that was opened.
    TextDocumentItem document;
};

}  // namespace clice::proto

namespace clice {

class CacheManager {
public:
    promise<void> buildPCH(std::string file, std::string content);

private:
    std::string outDir;
};

class LSPServer {
public:
    promise<void> onInitialize(json::Value id, const proto::InitializeParams& params);

    promise<void> onInitialized(const proto::InitializedParams& params);

    promise<void> onShutdown(json::Value id);

    promise<void> onExit();

public:
    promise<void> onDidOpen(const proto::DidOpenTextDocumentParams& document);

public:
    LSPServer() {
        addRequest("initialize", &LSPServer::onInitialize);
        addNotification("initialized", &LSPServer::onInitialized);
        addRequest("shutdown", &LSPServer::onShutdown);
        addNotification("exit", &LSPServer::onExit);
        addNotification("textDocument/didOpen", &LSPServer::onDidOpen);
    }

    template <typename Param>
    void addRequest(llvm::StringRef name,
                    promise<void> (LSPServer::*method)(json::Value, const Param&)) {
        requests.try_emplace(name,
                             [this, method](json::Value id, json::Value value) -> promise<void> {
                                 co_await (this->*method)(std::move(id),
                                                          json::deserialize<Param>(value));
                             });
    }

    void addRequest(llvm::StringRef name, promise<void> (LSPServer::*method)(json::Value)) {
        requests.try_emplace(name,
                             [this, method](json::Value id, json::Value value) -> promise<void> {
                                 co_await (this->*method)(std::move(id));
                             });
    }

    template <typename Param>
    void addNotification(llvm::StringRef name, promise<void> (LSPServer::*method)(const Param&)) {
        notifications.try_emplace(name, [this, method](json::Value value) -> promise<void> {
            co_await (this->*method)(json::deserialize<Param>(value));
        });
    }

    void addNotification(llvm::StringRef name, promise<void> (LSPServer::*method)()) {
        notifications.try_emplace(name, [this, method](json::Value value) -> promise<void> {
            co_await (this->*method)();
        });
    }

    promise<void> dispatch(json::Value value, Writer& writer);

private:
    using onRequest = llvm::unique_function<promise<void>(json::Value, json::Value)>;
    using onNotification = llvm::unique_function<promise<void>(json::Value)>;

    Writer* writer;
    llvm::StringMap<onRequest> requests;
    llvm::StringMap<onNotification> notifications;
};

}  // namespace clice
