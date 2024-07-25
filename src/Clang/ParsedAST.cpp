#include <Clang/ParsedAST.h>
#include <Support/Logger.h>

namespace clice {

std::unique_ptr<ParsedAST> ParsedAST::build(std::string_view path, std::string_view content) {}

std::unique_ptr<ParsedAST> ParsedAST::build(std::string_view path,
                                            const std::shared_ptr<CompilerInvocation>& invocation,
                                            const Preamble& preamble) {}

}  // namespace clice
