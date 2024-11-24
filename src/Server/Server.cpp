#include "Server/Server.h"
#include "Basic/URI.h"

namespace clice {

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
    addMethod("textDocument/semanticTokens", &Server::onSemanticTokens);
    addMethod("textDocument/inlayHint", &Server::onInlayHint);
    addMethod("textDocument/completion", &Server::onCodeCompletion);
    addMethod("textDocument/signatureHelp", &Server::onSignatureHelp);
    addMethod("textDocument/codeAction", &Server::onCodeAction);
    addMethod("textDocument/formatting", &Server::onFormatting);
    addMethod("textDocument/rangeFormatting", &Server::onRangeFormatting);
}

void Server::run(int argc, const char** argv) {
    logger::init("console", argv[0]);

    auto dispatch = [this](json::Value value) -> async::promise<void> {
        assert(value.kind() == json::Value::Object);
        auto object = value.getAsObject();
        assert(object && "value is not an object");
        if(auto method = object->get("method")) {
            auto name = *method->getAsString();
            auto params = object->get("params");
            if(auto id = object->get("id")) {
                if(auto iter = requests.find(name); iter != requests.end()) {
                    logger::info("Request: {0}", name.str());
                    co_await iter->second(std::move(*id),
                                          params ? std::move(*params) : json::Value(nullptr));
                } else {
                    logger::error("Unknown request: {0}", name.str());
                }
            } else {
                if(auto iter = notifications.find(name); iter != notifications.end()) {
                    logger::info("Notification: {0}", name.str());
                    co_await iter->second(params ? std::move(*params) : json::Value(nullptr));
                } else {
                    logger::error("Unknown notification: {0}", name.str());
                }
            }
        }

        co_return;
    };

    async::start_server(dispatch, "127.0.0.1", 50051);
}

async::promise<void> Server::onInitialize(json::Value id, const proto::InitializeParams& params) {
    async::write(std::move(id), json::serialize(proto::InitializeResult()));
    co_return;
}

async::promise<void> Server::onInitialized(const proto::InitializedParams& params) {
    co_return;
}

async::promise<void> Server::onExit(const proto::None&) {
    co_return;
}

async::promise<void> Server::onShutdown(json::Value id, const proto::None&) {
    co_return;
}

async::promise<void> Server::onDidOpen(const proto::DidOpenTextDocumentParams& params) {
    auto path = URI::resolve(params.textDocument.uri);
    llvm::StringRef content = params.textDocument.text;

    co_await buildAST(path, content);

    co_return;
}

async::promise<void> Server::onDidChange(const proto::DidChangeTextDocumentParams& document) {
    auto path = URI::resolve(document.textDocument.uri);
    llvm::StringRef content = document.contentChanges[0].text;

    co_await buildAST(path, content);
    co_return;
}

async::promise<void> Server::onDidSave(const proto::DidSaveTextDocumentParams& document) {
    co_return;
}

async::promise<void> Server::onDidClose(const proto::DidCloseTextDocumentParams& document) {
    co_return;
}

async::promise<void> Server::onGotoDeclaration(json::Value id,
                                               const proto::DeclarationParams& params) {
    co_return;
}

async::promise<void> Server::onGotoDefinition(json::Value id,
                                              const proto::DefinitionParams& params) {
    co_return;
}

async::promise<void> Server::onGotoTypeDefinition(json::Value id,
                                                  const proto::TypeDefinitionParams& params) {
    co_return;
}

async::promise<void> Server::onGotoImplementation(json::Value id,
                                                  const proto::ImplementationParams& params) {
    co_return;
}

async::promise<void> Server::onFindReferences(json::Value id,
                                              const proto::ReferenceParams& params) {
    co_return;
}

async::promise<void>
    Server::onPrepareCallHierarchy(json::Value id,
                                   const proto::CallHierarchyPrepareParams& params) {
    co_return;
}

async::promise<void> Server::onIncomingCall(json::Value id,
                                            const proto::CallHierarchyIncomingCallsParams& params) {
    co_return;
}

async::promise<void> Server::onOutgoingCall(json::Value id,
                                            const proto::CallHierarchyOutgoingCallsParams& params) {
    co_return;
}

async::promise<void>
    Server::onPrepareTypeHierarchy(json::Value id,
                                   const proto::TypeHierarchyPrepareParams& params) {
    co_return;
}

async::promise<void> Server::onSupertypes(json::Value id,
                                          const proto::TypeHierarchySupertypesParams& params) {
    co_return;
}

async::promise<void> Server::onSubtypes(json::Value id,
                                        const proto::TypeHierarchySubtypesParams& params) {
    co_return;
}

async::promise<void> Server::onDocumentHighlight(json::Value id,
                                                 const proto::DocumentHighlightParams& params) {
    co_return;
}

async::promise<void> Server::onDocumentLink(json::Value id,
                                            const proto::DocumentLinkParams& params) {
    co_return;
}

async::promise<void> Server::onHover(json::Value id, const proto::HoverParams& params) {
    co_return;
}

async::promise<void> Server::onCodeLens(json::Value id, const proto::CodeLensParams& params) {
    co_return;
}

async::promise<void> Server::onFoldingRange(json::Value id,
                                            const proto::FoldingRangeParams& params) {
    co_return;
}

async::promise<void> Server::onDocumentSymbol(json::Value id,
                                              const proto::DocumentSymbolParams& params) {
    co_return;
}

async::promise<void> Server::onSemanticTokens(json::Value id,
                                              const proto::SemanticTokensParams& params) {

    co_return;
}

async::promise<void> Server::onInlayHint(json::Value id, const proto::InlayHintParams& params) {
    co_return;
}

async::promise<void> Server::onCodeCompletion(json::Value id,
                                              const proto::CompletionParams& params) {
    co_return;
}

async::promise<void> Server::onSignatureHelp(json::Value id,
                                             const proto::SignatureHelpParams& params) {
    co_return;
}

async::promise<void> Server::onCodeAction(json::Value id, const proto::CodeActionParams& params) {
    co_return;
}

async::promise<void> Server::onFormatting(json::Value id,
                                          const proto::DocumentFormattingParams& params) {
    co_return;
}

async::promise<void> Server::onRangeFormatting(json::Value id,
                                               const proto::DocumentRangeFormattingParams& params) {
    co_return;
}

async::promise<void> Server::updatePCH(llvm::StringRef filepath,
                                       llvm::StringRef content,
                                       llvm::ArrayRef<const char*> args) {
    logger::info("Start building PCH for {0}", filepath.str());
    clang::PreambleBounds bounds = {0, 0};
    std::string outpath = "/home/ykiko/C++/clice2/build/cache/xxx.pch";
    co_await async::schedule_task([&] {
        Compiler compiler(filepath, content, args);
        bounds = clang::Lexer::ComputePreamble(content, {}, false);
        compiler.generatePCH(outpath, bounds.Size, bounds.PreambleEndsAtStartOfLine);
    });
    logger::info("Build PCH success");

    auto preamble2 = content.substr(0, bounds.Size).str();
    if(bounds.PreambleEndsAtStartOfLine) {
        preamble2.append("@");
    }

    pchs.try_emplace(filepath, PCH{.path = outpath, .preamble = std::move(preamble2)});
    co_return;
}

async::promise<void> Server::buildAST(llvm::StringRef filepath, llvm::StringRef content) {
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
    co_await (isModule ? updatePCM() : updatePCH(filepath, content, args));

    auto& pch = pchs.at(filepath);

    logger::info("Start building AST for {0}", filepath.str());
    auto compiler = co_await async::schedule_task([&] {
        std::uint32_t boundSize = pch.preamble.size();
        bool endAtStart = false;
        if(pch.preamble.back() == '@') {
            boundSize -= 1;
            endAtStart = true;
        }

        std::unique_ptr<Compiler> compiler = std::make_unique<Compiler>(path, content, args);
        compiler->applyPCH(pch.path, boundSize, endAtStart);
        compiler->buildAST();
        return compiler;
    });
    logger::info("Build AST success");

    auto& unit = units[filepath];
    unit.state = TranslationUnit::State::Ready;
    unit.compiler = std::move(compiler);

    for(auto& task: unit.tasks) {
        co_await task.request;
        if(task.kind == TranslationUnit::TaskKind::Build) {
            break;
        }
    }
}
}  // namespace clice
