#pragma once

#include <Support/Async.h>
#include <Clang/ParsedAST.h>

namespace clice {

struct TranslationUnit {
    std::string path;
    std::deque<std::string> messages;
    std::unique_ptr<ParsedAST> ast;
};

/// a class used to manage all resources for the LSP server
class Scheduler {
private:
    llvm::StringMap<std::unique_ptr<ParsedAST>> parsedASTs;

public:
    // update the AST for a given file
    Task<void> update(std::string_view path, std::string_view content);
};

}  // namespace clice
