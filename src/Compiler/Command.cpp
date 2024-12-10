#include "Compiler/Command.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/raw_ostream.h"

namespace clice {

llvm::Error mangleCommand(llvm::StringRef command,
                          llvm::SmallVectorImpl<const char*>& out,
                          llvm::SmallVectorImpl<char>& buffer) {
    llvm::SmallString<128> current;
    llvm::SmallVector<uint32_t> indices;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;

    for(size_t i = 0; i < command.size(); ++i) {
        char c = command[i];
        if(c == ' ' && !inSingleQuote && !inDoubleQuote) {
            if(!current.empty()) {
                indices.push_back(buffer.size());
                buffer.append(current);
                buffer.push_back('\0');
                current.clear();
            }
        } else if(c == '\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
        } else if(c == '"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
        } else {
            current.push_back(c);
        }
    }

    if(!current.empty()) {
        indices.push_back(buffer.size());
        buffer.append(current);
        buffer.push_back('\0');
    }

    /// Add resource directory.
    indices.push_back(buffer.size());
    current = std::format("-resource-dir={}", fs::resource_dir);
    buffer.append(current);
    buffer.push_back('\0');

    /// FIXME: use better way to remove args.
    for(size_t i = 0; i < indices.size(); ++i) {
        llvm::StringRef arg(buffer.data() + indices[i]);

        /// Skip `-c` and `-o` arguments.
        if(arg == "-c") {
            continue;
        }

        if(arg.starts_with("-o")) {
            if(arg == "-o") {
                ++i;
            }
            continue;
        }

        /// TODO: remove PCH.

        out.push_back(arg.data());
    }

    return llvm::Error::success();
}

}  // namespace clice
