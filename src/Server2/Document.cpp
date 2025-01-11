#include "Basic/SourceConverter.h"
#include "Server/Server.h"

namespace clice {

async::promise<void> Server::onDidOpen(const proto::DidOpenTextDocumentParams& params) {
    auto path = SourceConverter::toPath(params.textDocument.uri);
    llvm::StringRef content = params.textDocument.text;
    co_await scheduler.update(path, content, synchronizer);
}

async::promise<void> Server::onDidChange(const proto::DidChangeTextDocumentParams& document) {
    auto path = SourceConverter::toPath(document.textDocument.uri);
    llvm::StringRef content = document.contentChanges[0].text;
    co_await scheduler.update(path, content, synchronizer);
}

async::promise<void> Server::onDidSave(const proto::DidSaveTextDocumentParams& document) {
    auto path = SourceConverter::toPath(document.textDocument.uri);
    /// co_await scheduler.save(path);
    co_return;
}

async::promise<void> Server::onDidClose(const proto::DidCloseTextDocumentParams& document) {
    auto path = SourceConverter::toPath(document.textDocument.uri);
    /// co_await scheduler.close(path);
    co_return;
}

}  // namespace clice
