#pragma once

#include <Server/Scheduler.h>
#include <llvm/ADT/FunctionExtras.h>
#include <Feature/CodeCompletion.h>

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
}  // namespace clice::proto

namespace clice {

struct Server {
    using Handler = llvm::unique_function<void(json::Value, json::Value)>;
    Scheduler scheduler;
    llvm::StringMap<Handler> handlers;

    static Server instance;

    Server();

    int run(int argc, const char** argv);

    void handleMessage(std::string_view message);

    void notify();

    void response(json::Value id, json::Value result);

public:
    // LSP methods, if the return type is void, the method is used for notification.
    // otherwise, the method is used for request.
    auto initialize(proto::InitializeParams params) -> proto::InitializeResult;

    void initialized(proto::InitializedParams params);
};

}  // namespace clice
