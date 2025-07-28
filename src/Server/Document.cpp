#include "Support/Logger.h"
#include "Server/Server.h"

namespace clice {

async::Task<> Server::on_did_open(proto::DidOpenTextDocumentParams params) {
    // auto path = converter.convert(params.textDocument.uri);
    // auto file =
    //     co_await scheduler.add_document(std::move(path), std::move(params.textDocument.text));
    //
    // if(file->diagnostics) {}
    co_return;
}

async::Task<> Server::on_did_change(proto::DidChangeTextDocumentParams params) {
    // auto path = converter.convert(params.textDocument.uri);
    // co_await scheduler.add_document(std::move(path), std::move(params.contentChanges[0].text));
    co_return;
}

async::Task<> Server::on_did_save(proto::DidSaveTextDocumentParams params) {
    co_return;
}

async::Task<> Server::on_did_close(proto::DidCloseTextDocumentParams params) {
    co_return;
}

}  // namespace clice
