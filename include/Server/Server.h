#pragma once

#include "Async.h"
#include "Config.h"
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
    Server() {
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
        addMethod("textDocument/semanticTokens", &Server::onSemanticTokens);
        addMethod("textDocument/inlayHint", &Server::onInlayHint);
        addMethod("textDocument/completion", &Server::onCodeCompletion);
        addMethod("textDocument/signatureHelp", &Server::onSignatureHelp);
        addMethod("textDocument/codeAction", &Server::onCodeAction);
        addMethod("textDocument/formatting", &Server::onFormatting);
        addMethod("textDocument/rangeFormatting", &Server::onRangeFormatting);
    }

    promise<void> dispatch(json::Value value);

    void run(int argc, const char** argv) {
        auto loop = [this](json::Value value) -> promise<void> {
            co_await dispatch(std::move(value));
        };
        async::start_server(loop, "127.0.0.1", 50051);
    }

private:
    using onRequest = llvm::unique_function<promise<void>(json::Value, json::Value)>;
    using onNotification = llvm::unique_function<promise<void>(json::Value)>;

    template <typename Param>
    void addMethod(llvm::StringRef name,
                   promise<void> (Server::*method)(json::Value, const Param&)) {
        requests.try_emplace(name,
                             [this, method](json::Value id, json::Value value) -> promise<void> {
                                 co_await (this->*method)(std::move(id),
                                                          json::deserialize<Param>(value));
                             });
    }

    template <typename Param>
    void addMethod(llvm::StringRef name, promise<void> (Server::*method)(const Param&)) {
        notifications.try_emplace(name, [this, method](json::Value value) -> promise<void> {
            co_await (this->*method)(json::deserialize<Param>(value));
        });
    }

private:
    /// ============================================================================
    ///                            Lifestyle Message
    /// ============================================================================

    promise<void> onInitialize(json::Value id, const proto::InitializeParams& params);

    promise<void> onInitialized(const proto::InitializedParams& params);

    promise<void> onShutdown(json::Value id, const proto::None&);

    promise<void> onExit(const proto::None&);

    /// ============================================================================
    ///                         Document Synchronization
    /// ============================================================================

    promise<void> onDidOpen(const proto::DidOpenTextDocumentParams& document);

    promise<void> onDidChange(const proto::DidChangeTextDocumentParams& document);

    promise<void> onDidSave(const proto::DidSaveTextDocumentParams& document);

    promise<void> onDidClose(const proto::DidCloseTextDocumentParams& document);

    /// ============================================================================
    ///                             Language Features
    /// ============================================================================

    promise<void> onGotoDeclaration(json::Value id, const proto::DeclarationParams& params);

    promise<void> onGotoDefinition(json::Value id, const proto::DefinitionParams& params);

    promise<void> onGotoTypeDefinition(json::Value id, const proto::TypeDefinitionParams& params);

    promise<void> onGotoImplementation(json::Value id, const proto::ImplementationParams& params);

    promise<void> onFindReferences(json::Value id, const proto::ReferenceParams& params);

    promise<void> onPrepareCallHierarchy(json::Value id,
                                         const proto::CallHierarchyPrepareParams& params);

    promise<void> onIncomingCall(json::Value id,
                                 const proto::CallHierarchyIncomingCallsParams& params);

    promise<void> onOutgoingCall(json::Value id,
                                 const proto::CallHierarchyOutgoingCallsParams& params);

    promise<void> onPrepareTypeHierarchy(json::Value id,
                                         const proto::TypeHierarchyPrepareParams& params);

    promise<void> onSupertypes(json::Value id, const proto::TypeHierarchySupertypesParams& params);

    promise<void> onSubtypes(json::Value id, const proto::TypeHierarchySubtypesParams& params);

    promise<void> onDocumentHighlight(json::Value id, const proto::DocumentHighlightParams& params);

    promise<void> onDocumentLink(json::Value id, const proto::DocumentLinkParams& params);

    promise<void> onHover(json::Value id, const proto::HoverParams& params);

    promise<void> onCodeLens(json::Value id, const proto::CodeLensParams& params);

    promise<void> onFoldingRange(json::Value id, const proto::FoldingRangeParams& params);

    promise<void> onDocumentSymbol(json::Value id, const proto::DocumentSymbolParams& params);

    promise<void> onSemanticTokens(json::Value id, const proto::SemanticTokensParams& params);

    promise<void> onInlayHint(json::Value id, const proto::InlayHintParams& params);

    promise<void> onCodeCompletion(json::Value id, const proto::CompletionParams& params);

    promise<void> onSignatureHelp(json::Value id, const proto::SignatureHelpParams& params);

    promise<void> onCodeAction(json::Value id, const proto::CodeActionParams& params);

    promise<void> onFormatting(json::Value id, const proto::DocumentFormattingParams& params);

    promise<void> onRangeFormatting(json::Value id,
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

    promise<void> updatePCH() {
        co_return;
    }

    promise<void> updatePCM() {
        co_return;
    }

    promise<void> buildAST(llvm::StringRef filepath, llvm::StringRef content) {
        llvm::SmallString<128> path = filepath;

        /// FIXME: lookup from CDB file and adjust and remove unnecessary arguments.
        llvm::SmallVector<const char*> args = {
            "clang++",
            "-std=c++20",
            path.c_str(),
            "-resource-dir",
            "/home/ykiko/C++/clice2/build/lib/clang/20",
        };

        /// through arguments to judge is it a module.
        bool isModule = false;
        co_await (isModule ? updatePCM() : updatePCH());

        auto compiler = co_await async::schedule_task([=]() {
            std::unique_ptr<Compiler> compiler = std::make_unique<Compiler>(path, content, args);
            compiler->buildAST();
            return compiler;
        });
    }

private:
    llvm::StringMap<onRequest> requests;
    llvm::StringMap<onNotification> notifications;
};

}  // namespace clice
