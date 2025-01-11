#include "Server/Server.h"

namespace clice {

async::promise<void> Server::onContextCurrent(const proto::TextDocumentIdentifier& params) {
    co_return;
}

async::promise<void> Server::onContextAll(const proto::TextDocumentIdentifier& params) {
    co_return;
}

async::promise<void> Server::onContextSwitch(const proto::TextDocumentIdentifier& params) {
    co_return;
}

}  // namespace clice
