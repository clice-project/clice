#include <string_view>

namespace clice {

/// Settings for the LSP server
struct Setting {
    /// compile_commands.json file path
    std::string compile_commands_dictionary = "/build";
    /// mode of the server: ["pipe", "socket", "index"]
    std::string mode = "pipe";
};

/// global setting instance
inline Setting setting = {};

}  // namespace clice
