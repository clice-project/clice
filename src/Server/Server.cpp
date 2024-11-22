#include "Server/Server.h"
#include "Basic/URI.h"

namespace clice {

promise<void> CacheManager::buildPCH(std::string file, std::string content) {

    co_return;
}

promise<void> LSPServer::onInitialize(json::Value id, const proto::InitializeParams& params) {
    llvm::outs() << "onInitialize\n";
    writer->write(std::move(id), json::serialize(proto::InitializeResult()));
    async::sleep(std::chrono::seconds(10));
    co_return;
}

promise<void> LSPServer::onInitialized(const proto::InitializedParams& params) {
    llvm::outs() << "onInitialized\n";
    co_return;
}

promise<void> LSPServer::onExit(const proto::None&) {
    llvm::outs() << "onExit\n";
    co_return;
}

promise<void> LSPServer::onShutdown(json::Value id, const proto::None&) {
    llvm::outs() << "onShutdown\n";
    co_return;
}

promise<void> LSPServer::onDidOpen(const proto::DidOpenTextDocumentParams& params) {
    llvm::outs() << "onDidOpen: " << params.textDocument.uri << "\n";
    auto path = URI::resolve(params.textDocument.uri);
    llvm::StringRef content = params.textDocument.text;

    co_await async::schedule_task([&]() {
        // TODO: lookup
        std::vector<const char*> args = {
            "clang++",
            "-std=c++20",
            path.c_str(),
            "-resource-dir",
            "/home/ykiko/C++/clice2/build/lib/clang/20",
        };
        Compiler compiler(path, content, args);

        auto bounds = clang::Lexer::ComputePreamble(content, {}, false);

        llvm::outs() << "Generating PCH\n";

        compiler.generatePCH("/home/ykiko/C++/clice2/build/cache/xxx.pch",
                             bounds.Size,
                             bounds.PreambleEndsAtStartOfLine);
    });

    llvm::outs() << "build PCH success\n";

    co_return;
}

promise<void> LSPServer::dispatch(json::Value value, Writer& writer) {
    this->writer = &writer;
    assert(value.kind() == json::Value::Object);
    auto object = value.getAsObject();
    assert(object && "value is not an object");
    if(auto method = object->get("method")) {
        auto name = *method->getAsString();
        if(auto id = object->get("id")) {
            if(auto iter = requests.find(name); iter != requests.end()) {
                co_await iter->second(std::move(*id), std::move(*object->get("params")));
            } else {
                llvm::errs() << "Unknown request: " << name << "\n";
            }
        } else {
            if(auto iter = notifications.find(name); iter != notifications.end()) {
                co_await iter->second(std::move(*object->get("params")));
            } else {
                llvm::errs() << "Unknown notification: " << name << "\n";
            }
        }
    }

    co_return;
}

}  // namespace clice
