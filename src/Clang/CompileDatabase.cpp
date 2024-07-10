
#include <Clang/CompileDatabase.h>
#include <Support/Logger.h>

namespace clice {

void CompileDatabase::load(std::string_view path) {
    std::string error;
    database = clang::tooling::CompilationDatabase::loadFromDirectory(path, error);
    if(!database) {
        logger::error("failed to load compilation database: {}", error);
    }
}

}  // namespace clice
