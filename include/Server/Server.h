#pragma once

#include "Async.h"
#include "Config.h"
#include "Logger.h"
#include "Basic/Document.h"
#include "Compiler/Compiler.h"
#include "Support/JSON.h"
#include "Support/FileSystem.h"

#include "Feature/Lookup.h"
#include "Feature/DocumentHighlight.h"
#include "Feature/DocumentLink.h"
#include "Feature/Hover.h"
#include "Feature/CodeLens.h"
#include "Feature/FoldingRange.h"
#include "Feature/DocumentSymbol.h"
#include "Feature/SemanticTokens.h"
#include "Feature/InlayHint.h"
#include "Feature/CodeCompletion.h"
#include "Feature/SignatureHelp.h"
#include "Feature/CodeAction.h"
#include "Feature/Formatting.h"

#include "llvm/ADT/FunctionExtras.h"

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

struct None {};

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
    SemanticTokensOptions semanticTokensProvider;
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
    TextDocumentItem textDocument;
};

struct DidChangeTextDocumentParams {
    /// The document that did change. The version number points
    /// to the version after all provided content changes have
    /// been applied.
    VersionedTextDocumentIdentifier textDocument;

    /// The actual content changes.
    std::vector<TextDocumentContentChangeEvent> contentChanges;
};

struct DidSaveTextDocumentParams {
    /// The document that was saved.
    TextDocumentIdentifier textDocument;

    /// Optional the content when saved. Depends on the includeText value
    /// when the save notifcation was requested.
    string text;
};

struct DidCloseTextDocumentParams {
    /// The document that was closed.
    TextDocumentIdentifier textDocument;
};

}  // namespace clice::proto

namespace clice {

class Server {
public:
    Server();

    void run(int argc, const char** argv);

private:
    using onRequest = llvm::unique_function<async::promise<void>(json::Value, json::Value)>;
    using onNotification = llvm::unique_function<async::promise<void>(json::Value)>;

    template <typename Param>
    void addMethod(llvm::StringRef name,
                   async::promise<void> (Server::*method)(json::Value, const Param&)) {
        requests.try_emplace(
            name,
            [this, method](json::Value id, json::Value value) -> async::promise<void> {
                co_await (this->*method)(std::move(id), json::deserialize<Param>(value));
            });
    }

    template <typename Param>
    void addMethod(llvm::StringRef name, async::promise<void> (Server::*method)(const Param&)) {
        notifications.try_emplace(name, [this, method](json::Value value) -> async::promise<void> {
            co_await (this->*method)(json::deserialize<Param>(value));
        });
    }

    llvm::StringMap<onRequest> requests;
    llvm::StringMap<onNotification> notifications;

private:
    /// ============================================================================
    ///                            Lifestyle Message
    /// ============================================================================

    async::promise<void> onInitialize(json::Value id, const proto::InitializeParams& params);

    async::promise<void> onInitialized(const proto::InitializedParams& params);

    async::promise<void> onShutdown(json::Value id, const proto::None&);

    async::promise<void> onExit(const proto::None&);

    /// ============================================================================
    ///                         Document Synchronization
    /// ============================================================================

    async::promise<void> onDidOpen(const proto::DidOpenTextDocumentParams& document);

    async::promise<void> onDidChange(const proto::DidChangeTextDocumentParams& document);

    async::promise<void> onDidSave(const proto::DidSaveTextDocumentParams& document);

    async::promise<void> onDidClose(const proto::DidCloseTextDocumentParams& document);

    /// ============================================================================
    ///                             Language Features
    /// ============================================================================

    async::promise<void> onGotoDeclaration(json::Value id, const proto::DeclarationParams& params);

    async::promise<void> onGotoDefinition(json::Value id, const proto::DefinitionParams& params);

    async::promise<void> onGotoTypeDefinition(json::Value id,
                                              const proto::TypeDefinitionParams& params);

    async::promise<void> onGotoImplementation(json::Value id,
                                              const proto::ImplementationParams& params);

    async::promise<void> onFindReferences(json::Value id, const proto::ReferenceParams& params);

    async::promise<void> onPrepareCallHierarchy(json::Value id,
                                                const proto::CallHierarchyPrepareParams& params);

    async::promise<void> onIncomingCall(json::Value id,
                                        const proto::CallHierarchyIncomingCallsParams& params);

    async::promise<void> onOutgoingCall(json::Value id,
                                        const proto::CallHierarchyOutgoingCallsParams& params);

    async::promise<void> onPrepareTypeHierarchy(json::Value id,
                                                const proto::TypeHierarchyPrepareParams& params);

    async::promise<void> onSupertypes(json::Value id,
                                      const proto::TypeHierarchySupertypesParams& params);

    async::promise<void> onSubtypes(json::Value id,
                                    const proto::TypeHierarchySubtypesParams& params);

    async::promise<void> onDocumentHighlight(json::Value id,
                                             const proto::DocumentHighlightParams& params);

    async::promise<void> onDocumentLink(json::Value id, const proto::DocumentLinkParams& params);

    async::promise<void> onHover(json::Value id, const proto::HoverParams& params);

    async::promise<void> onCodeLens(json::Value id, const proto::CodeLensParams& params);

    async::promise<void> onFoldingRange(json::Value id, const proto::FoldingRangeParams& params);

    async::promise<void> onDocumentSymbol(json::Value id,
                                          const proto::DocumentSymbolParams& params);

    async::promise<void> onSemanticTokens(json::Value id,
                                          const proto::SemanticTokensParams& params);

    async::promise<void> onInlayHint(json::Value id, const proto::InlayHintParams& params);

    async::promise<void> onCodeCompletion(json::Value id, const proto::CompletionParams& params);

    async::promise<void> onSignatureHelp(json::Value id, const proto::SignatureHelpParams& params);

    async::promise<void> onCodeAction(json::Value id, const proto::CodeActionParams& params);

    async::promise<void> onFormatting(json::Value id,
                                      const proto::DocumentFormattingParams& params);

    async::promise<void> onRangeFormatting(json::Value id,
                                           const proto::DocumentRangeFormattingParams& params);

private:
    /// Information of building precompiled header.
    struct PCH {
        /// The path of this PCH.
        std::string path;
        /// The source file path.
        std::string sourcePath;
        /// The header part of source file used to build this PCH.
        std::string preamble;
        /// The arguments used to build this PCH.
        std::string arguments;
        /// All files involved in building this PCH(excluding the source file).
        std::vector<std::string> deps;

        /// FIXME: use asyncronous file system API.
        bool needUpdate(llvm::StringRef sourceContent) {
            /// Check whether the header part changed.
            if(sourceContent.substr(0, preamble.size()) != preamble) {
                return true;
            }

            /// Check timestamp of all files involved in building this PCH.
            fs::file_status build;
            if(auto error = fs::status(path, build)) {
                llvm::errs() << "Error: " << error.message() << "\n";
                std::terminate();
            }

            /// TODO: check whether deps changed through comparing timestamps.
            return false;
        }
    };

    /// Information of building precompiled module.
    struct PCM {};

    async::promise<void> updatePCH(llvm::StringRef filepath,
                                   llvm::StringRef content,
                                   llvm::ArrayRef<const char*> args);

    async::promise<void> updatePCM() {
        co_return;
    }

    async::promise<void> buildAST(llvm::StringRef filepath, llvm::StringRef content);

    struct TranslationUnit {
        enum class State {
            Building,
            Ready,
        };

        enum class TaskKind {
            Build,
            Consume,
        };

        struct Task {
            TaskKind kind;
            llvm::unique_function<async::promise<void>(Compiler&)> request;
        };

        State state;
        std::unique_ptr<Compiler> compiler;
        std::vector<Task> tasks;
    };

    /// Schedule a task for a file. If the file is building, the task will be
    /// appended to the task list of the file and wait for the building to finish.
    /// Otherwise, the task will be executed immediately.
    async::promise<void> schedule(llvm::StringRef path,
                                  llvm::unique_function<async::promise<void>(Compiler&)> callback);

private:
    llvm::StringMap<PCH> pchs;
    llvm::StringMap<TranslationUnit> units;
};

}  // namespace clice
