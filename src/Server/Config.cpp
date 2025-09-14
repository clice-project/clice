#define TOML_EXCEPTIONS 0
#include "toml++/toml.hpp"

#include "Server/Config.h"
#include "Support/Logging.h"
#include "Support/FileSystem.h"
#include "llvm/ADT/StringMap.h"

namespace clice::config {

static llvm::StringMap<std::string> predefined = {
    /// the directory of the workplace.
    {"workplace",    ""     },
    /// the directory of the executable.
    {"binary",       ""     },
    /// the version of the clice.
    {"version",      "0.0.1"},
    /// the version of dependent llvm.
    {"llvm_version", "20"   },
};

/// predefined variables.
llvm::StringRef version = predefined["version"];
llvm::StringRef binary = predefined["binary"];
llvm::StringRef llvm_version = predefined["llvm_version"];
llvm::StringRef workspace = predefined["workplace"];

struct Config {
    ServerOptions server;
    CacheOptions cache;
    IndexOptions index;
    std::vector<Rule> rules;
};

/// global config instance.
static Config config = {};

const ServerOptions& server = config.server;
const CacheOptions& cache = config.cache;
const IndexOptions& index = config.index;
llvm::ArrayRef<Rule> rules = config.rules;

template <typename Object>
static void parse(Object& object, auto&& value) {
    if constexpr(std::is_same_v<Object, bool>) {
        if(auto v = value.as_boolean()) {
            object = v->get();
        }
    } else if constexpr(clice::integral<Object>) {
        if(auto v = value.as_integer()) {
            object = v->get();
        }
    } else if constexpr(std::is_same_v<Object, std::string>) {
        if(auto v = value.as_string()) {
            object = v->get();
        }
    } else if constexpr(clice::is_specialization_of<Object, std::vector>) {
        if(auto v = value.as_array()) {
            for(auto& item: *v) {
                object.emplace_back();
                parse(object.back(), item);
            }
        }
    } else if constexpr(refl::reflectable_struct<Object>) {
        if(auto table = value.as_table()) {
            refl::foreach(object, [&](std::string_view key, auto& member) {
                if(auto v = (*table)[key]) {
                    parse(member, v);
                }
            });
        }
    } else {
        static_assert(dependent_false<Object>, "Unsupported type");
    }
}

std::expected<void, std::string> load(llvm::StringRef execute, llvm::StringRef filename) {
    predefined["version"] = "0.0.1";
    predefined["binary"] = execute;
    predefined["llvm_version"] = "20";

    auto toml = toml::parse_file(filename);
    if(toml.failed()) {
        return std::unexpected<std::string>(toml.error().description());
    }

    parse(config, toml.table());
    return {};
}

/// replace all predefined variables in the text.
static void resolve(std::string& input) {
    std::string_view text = input;
    llvm::SmallString<128> path;
    std::size_t pos = 0;
    while((pos = text.find("${", pos)) != std::string::npos) {
        path.append(text.substr(0, pos));

        auto end = text.find('}', pos + 2);
        if(end == std::string::npos) {
            path.append(text.substr(pos));
            break;
        }

        auto variable = text.substr(pos + 2, end - (pos + 2));

        if(auto iter = predefined.find(variable); iter != predefined.end()) {
            path.append(iter->second);
        } else {
            path.append(text.substr(pos, end - pos + 1));
        }

        text.remove_prefix(end + 1);
        pos = 0;
    }

    path.append(text);
    path::remove_dots(path, true);
    input = path.str();
}

template <typename Object>
static void replace(Object& object) {
    if constexpr(std::is_same_v<Object, std::string>) {
        resolve(object);
    } else if constexpr(clice::is_specialization_of<Object, std::vector>) {
        for(auto& item: object) {
            replace(item);
        }
    } else if constexpr(refl::reflectable_struct<Object>) {
        refl::foreach(object, [&](auto, auto& member) { replace(member); });
    }
}

void init(std::string_view workplace) {
    predefined["workspace"] = workplace;

    replace(config);

    logging::info("Config initialized successfully: {0}", json::serialize(config));
    return;
}

}  // namespace clice::config
