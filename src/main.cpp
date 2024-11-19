#include "Server/Server.h"
#include "llvm/Support/CommandLine.h"

namespace clice {}

using namespace clice;

int main(int argc, const char** argv) {
    llvm::cl::opt<std::string> config("config");
    llvm::cl::ParseCommandLineOptions(argc, argv);

    if(!config.hasArgStr()) {
        llvm::errs() << "Missing config file.\n";
        std::terminate();
    }

    llvm::outs() << "Config file: " << config << "\n";

    config::parse(argv[0], config);

    Server server(config::server());
    auto loop = [&server]() {
        if(server.hasMessage()) {
            auto& json = server.peek();
            assert(json.kind() == json::Value::Object);
            auto object = json.getAsObject();

            if(auto method = object->get("method")) {
                assert(method->kind() == json::Value::String);
                auto name = method->getAsString();
                if(name == "initialize") {
                    assert(object->get("params"));
                    auto params =
                        json::deserialize<proto::InitializeParams>(*object->get("params"));
                    llvm::outs() << "Initialize: " << params.workspaceFolders[0].uri << "\n";
                    server.response(std::move(*object->get("id")),
                                    json::serialize(proto::InitializeResult{}));
                    server.consume();
                }
            }
        }
    };
    server.run(loop);
    return 0;
}
