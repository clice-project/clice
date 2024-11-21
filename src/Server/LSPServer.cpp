#include "Server/LSPServer.h"

namespace clice {

void LSPServer::dispatch(json::Value request) {
    assert(request.kind() == json::Value::Object);
    auto object = request.getAsObject();

    if(auto method = object->get("method")) {
        assert(method->kind() == json::Value::String);
        auto name = method->getAsString();
        if(auto iter = methods.find(*name); iter != methods.end()) {
            response(std::move(*object->get("id")),
                     iter->second(std::move(*object->get("params"))));
            consume();
        } else if(auto iter = notifications.find(*name); iter != notifications.end()) {
            iter->second(std::move(*object->get("params")));
            consume();
        } else {
            llvm::errs() << "Unknown method: " << *name << "\n";
            consume();
        }
    }
}

}  // namespace clice
