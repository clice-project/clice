#include <llvm/ADT/StringMap.h>
#include <Support/JSON.h>
#include <AST/Preamble.h>
#include <AST/ParsedAST.h>

namespace clice {

/// responsible for file management and scheduling tasks.
class Scheduler {
private:
    struct TU {
        using message = llvm::unique_function<json::Object()>;
        std::string content;
        std::deque<message> messages;
        std::unique_ptr<ParsedAST> ast;
        std::unique_ptr<Preamble> preamble;
    };

public:
    void dispatch(std::string_view method, json::Value value);

private:
    llvm::StringMap<TU> files;
};

}  // namespace clice