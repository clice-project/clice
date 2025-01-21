#include "Basic/SourceCode.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"

namespace clice {

llvm::StringRef getFileContent(const clang::SourceManager& SM, clang::FileID fid) {
    return SM.getBufferData(fid);
}

std::uint32_t getTokenLength(const clang::SourceManager& SM, clang::SourceLocation location) {
    return clang::Lexer::MeasureTokenLength(location, SM, {});
}

llvm::StringRef getTokenSpelling(const clang::SourceManager& SM, clang::SourceLocation location) {
    return llvm::StringRef(SM.getCharacterData(location), getTokenLength(SM, location));
}

}  // namespace clice
