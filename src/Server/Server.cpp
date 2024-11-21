#include "Server/Server.h"

namespace clice {

promise<void> CacheManager::buildPCH(std::string file, std::string content) {
    co_await async::schedule_task([&]() {
        // TODO: lookup
        std::vector<const char*> args = {
            "clang++",
            "-std=c++20",
            file.c_str(),
            "-resource-dir",
            "/home/ykiko/C++/clice2/build/lib/clang/20",
        };
        Compiler compiler(file, content, args);

        auto bounds = clang::Lexer::ComputePreamble(content, compiler.pp().getLangOpts(), false);

        llvm::outs() << "Generating PCH\n";

        compiler.generatePCH("/home/ykiko/C++/clice2/build/cache/xxx.pch",
                             bounds.Size,
                             bounds.PreambleEndsAtStartOfLine);
    });

    co_return;
}

promise<void> LSPServer::onInitialize(json::Value id, const proto::InitializeParams& params) {
    llvm::outs() << "onInitialize\n";
    writer->write(std::move(id), json::serialize(proto::InitializeResult()));
    co_return;
}

promise<void> LSPServer::onInitialized(const proto::InitializedParams& params) {
    llvm::outs() << "onInitialized\n";
    co_return;
}

promise<void> LSPServer::onExit() {
    llvm::outs() << "onExit\n";
    co_return;
}

promise<void> LSPServer::onShutdown(json::Value id) {
    llvm::outs() << "onShutdown\n";
    co_return;
}

promise<void> LSPServer::onDidOpen(const proto::DidOpenTextDocumentParams& params) {
    llvm::outs() << "onDidOpen\n";
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
                co_await iter->second(std::move(*id), std::move(*object));
            } else {
                llvm::errs() << "Unknown request: " << name << "\n";
            }
        } else {
            if(auto iter = notifications.find(name); iter != notifications.end()) {
                co_await iter->second(std::move(*object));
            } else {
                llvm::errs() << "Unknown notification: " << name << "\n";
            }
        }
    }

    co_return;
}

}  // namespace clice
