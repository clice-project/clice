#include "Basic/SourceConverter.h"
#include "Server/Server.h"

namespace clice {

async::Task<> Server::onDidOpen(const proto::DidOpenTextDocumentParams& params) {
    co_return;
}

async::Task<> Server::onDidChange(const proto::DidChangeTextDocumentParams& document) {
    co_return;
}

async::Task<> Server::onDidSave(const proto::DidSaveTextDocumentParams& document) {
    co_return;
}

async::Task<> Server::onDidClose(const proto::DidCloseTextDocumentParams& document) {
    co_return;
}

}  // namespace clice
