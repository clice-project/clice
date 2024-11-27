
#include <toml++/toml.hpp>

#include <Server/Config.h>
#include <Server/Logger.h>
#include <Support/Reflection.h>
#include <Support/FileSystem.h>
#include <llvm/ADT/StringMap.h>

#include "Support/Format.h"
#include "Support/JSON.h"

namespace clice::config {

namespace {

/// predefined variables.
llvm::StringMap<std::string> predefined = {
    /// the directory of the workplace.
    {"workplace",    ""     },
    /// the directory of the executable.
    {"binary",       ""     },
    /// the version of the clice.
    {"version",      "0.0.1"},
    /// the version of dependent llvm.
    {"llvm_version", "20"   },
};

struct Config {
    ServerOption server;
    FrontendOption frontend;
};

/// global config instance.
Config config = {};

/// replace all predefined variables in the text.
std::string replace(std::string_view text) {
    std::string result;
    std::size_t pos = 0;
    while((pos = text.find("${", pos)) != std::string::npos) {
        result.append(text.substr(0, pos));

        auto end = text.find('}', pos + 2);
        if(end == std::string::npos) {
            result.append(text.substr(pos));
            break;
        }

        auto variable = text.substr(pos + 2, end - (pos + 2));

        if(auto iter = predefined.find(variable); iter != predefined.end()) {
            result.append(iter->second);
        } else {
            result.append(text.substr(pos, end - pos + 1));
        }

        text.remove_prefix(end + 1);
        pos = 0;
    }

    result.append(text);
    return result;
}

}  // namespace

int parse(llvm::StringRef execute, llvm::StringRef filepath) {
    auto toml = toml::parse_file(filepath);
    if(toml.failed()) {
        log::fatal("Failed to parse config file: {0}, Beacuse {1}",
                   filepath,
                   toml.error().description());
    }

    auto table = toml["server"];
    if(table) {
        if(auto mode = table["mode"]) {
            config.server.mode = mode.as_string()->get();
        }

        if(auto port = table["port"]) {
            config.server.port = port.as_integer()->get();
        }

        if(auto address = table["address"]) {
            config.server.address = address.as_string()->get();
        }

        llvm::outs() << "Server:" << json::serialize(config.server) << "\n";
    }
    return 0;
}

void init(std::string_view workplace) {
    predefined["workplace"] = workplace;

    refl::walk(config, [&]<typename Field>(std::string_view name, Field& field) {
        if constexpr(std::is_same_v<Field, std::string>) {
            field = replace(field);
        }
    });
    return;
}

const ServerOption& server() {
    return config.server;
}

const FrontendOption& frontend() {
    return config.frontend;
}

}  // namespace clice::config
