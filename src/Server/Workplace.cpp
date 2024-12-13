#include "Server/Server.h"

namespace clice {

async::promise<> Server::onDidChangeWatchedFiles(const proto::DidChangeWatchedFilesParams& params) {
    for(auto& event: params.changes) {
        switch(event.type) {
            case proto::FileChangeType::Created: {
                break;
            }

            case proto::FileChangeType::Changed: {
                auto path = URI::resolve(event.uri);
                synchronizer.sync(path);
                break;
            }

            case proto::FileChangeType::Deleted: {
                break;
            }
        }
    }

    co_return;
}

}  // namespace clice
