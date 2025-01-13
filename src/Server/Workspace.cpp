#include "Basic/SourceConverter.h"
#include "Server/Server.h"

namespace clice {

async::Task<> Server::onDidChangeWatchedFiles(const proto::DidChangeWatchedFilesParams& params) {
    co_return;
}

}  // namespace clice
