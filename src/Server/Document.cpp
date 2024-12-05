#include "Server/Server.h"

namespace clice {

async::promise<void> Server::onDidOpen(const proto::DidOpenTextDocumentParams& params) {
    auto path = URI::resolve(params.textDocument.uri);
    llvm::StringRef content = params.textDocument.text;

    co_await scheduler.add(path, content);
}

async::promise<void> Server::onDidChange(const proto::DidChangeTextDocumentParams& document) {
    auto path = URI::resolve(document.textDocument.uri);
    llvm::StringRef content = document.contentChanges[0].text;
    co_await scheduler.update(path, content);
}

async::promise<void> Server::onDidSave(const proto::DidSaveTextDocumentParams& document) {
    auto path = URI::resolve(document.textDocument.uri);
    co_await scheduler.save(path);
    co_return;
}

async::promise<void> Server::onDidClose(const proto::DidCloseTextDocumentParams& document) {
    auto path = URI::resolve(document.textDocument.uri);
    co_await scheduler.close(path);
    co_return;
}

}  // namespace clice
