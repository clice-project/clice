#pragma once

namespace clice {

enum class Encoding {
    UTF8,
    UTF16,
    UTF32,
};

std::size_t length(llvm::StringRef text, Encoding encoding);

}  // namespace clice
