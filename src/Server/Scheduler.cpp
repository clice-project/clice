#include <Server/Scheduler.h>
#include <Protocol/Protocol.h>
#include <spdlog/spdlog.h>
#include <Feature/SemanticTokens.h>
#include <Server/Server.h>

namespace clice {

void Scheduler::dispatch(json::Value id, std::string_view method, json::Value value) {
    std::vector<const char*> compileArgs = {
        "clang++",
        "-std=c++20",
        "main.cpp",
        "-resource-dir=/home/ykiko/C++/clice2/build/lib/clang/20",
    };
    if(method == "textDocument/didOpen") {
        // auto params = json::deserialize<proto::DidOpenTextDocumentParams>(value);
        // auto AST = ParsedAST::build("main.cpp", params.textDocument.text, compileArgs);
        // files[params.textDocument.uri].ast = std::move(AST);
    } else if(method == "textDocument/didChange") {
        // auto params = json::deserialize<proto::DidChangeTextDocumentParams>(value);
        // auto AST = ParsedAST::build("main.cpp", params.contentChanges.back().text, compileArgs);
        // files[params.textDocument.uri].ast = std::move(AST);
    } else if(method == "textDocument/semanticTokens/full") {
        // auto params = json::deserialize<proto::SemanticTokensParams>(value);
        // auto AST = files[params.textDocument.uri].ast.get();
        // auto semanticTokens = feature::semanticTokens(*AST, "main.cpp");
        // Server::instance.response(std::move(id), json::serialize(semanticTokens));
    } else {
        spdlog::error("unknown method: {}", method);
    }
}

}  // namespace clice
