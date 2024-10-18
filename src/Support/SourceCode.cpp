#include <Support/SourceCode.h>

namespace clice {

std::size_t length(llvm::StringRef text, Encoding encoding) {
    switch(encoding) {
        case Encoding::UTF8: return text.size();
        case Encoding::UTF16: {
            // TODO:
            return text.size() / 2;
        }

        case Encoding::UTF32: {
            return text.size() / 4;
        }
    }
}

}  // namespace clice
