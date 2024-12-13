#include "Server/Server.h"

namespace clice {

async::promise<> Server::onDidChangeWatchedFiles(const proto::DidChangeWatchedFilesParams& params) {
    for(auto& event: params.changes) {
        switch(event.type) {
            case proto::FileChangeType::Created:
            case proto::FileChangeType::Changed: {
                auto path = URI::resolve(event.uri);
                auto buffer = llvm::MemoryBuffer::getFile(path);
                if(!buffer) {
                    log::fatal("Failed to read file {0}", path);
                }
                scheduler.update(path, buffer.get()->getBuffer());
            }
            case proto::FileChangeType::Deleted: break;
        }
    }
    co_return;
}

}  // namespace clice
