#include <Server/Scheduler.h>
#include <Protocol/Protocol.h>
#include <spdlog/spdlog.h>

namespace clice {

void Scheduler::dispatch(std::string_view method, json::Value value) {
    if(method == "textDocument/didOpen") {
        auto params = json::deserialize<protocol::DidOpenTextDocumentParams>(value);
    } else if(method == "textDocument/didChange") {
        auto params = json::deserialize<protocol::DidChangeTextDocumentParams>(value);
    } else if(method == "textDocument/semanticTokens/full") {
        auto params = json::deserialize<protocol::SemanticTokensParams>(value);
    } else {
        spdlog::error("unknown method: {}", method);
    }
}

}  // namespace clice
