#include "LSP/Scheduler.h"
#include "Clang/ParsedAST.h"

namespace clice {

Task<void> Scheduler::update(std::string_view name, std::string_view content) {
    auto ast = co_await async([=] {
        return ParsedAST::build(name, content);
    });

    parsedASTs[name] = std::move(ast);
}

}  // namespace clice
