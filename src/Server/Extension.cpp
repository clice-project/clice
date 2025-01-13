#include "Server/Server.h"

namespace clice {

async::Task<> Server::onContextCurrent(const proto::TextDocumentIdentifier& params) {
    co_return;
}

async::Task<> Server::onContextAll(const proto::TextDocumentIdentifier& params) {
    co_return;
}

async::Task<> Server::onContextSwitch(const proto::TextDocumentIdentifier& params) {
    co_return;
}

}  // namespace clice
