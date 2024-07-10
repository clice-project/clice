#include "LSP/Scheduler.h"
#include "Clang/ParsedAST.h"
#include "Support/Logger.h"

namespace clice {

Task<void> Scheduler::update(std::string_view name, std::string_view content) {
    auto ast = co_await async([=] {
        logger::info("start building AST for {}", name);
        return ParsedAST::build(name, content);
    });

    parsedASTs[name] = std::move(ast);
    logger::info("finished building AST for {}", name);
}

}  // namespace clice
