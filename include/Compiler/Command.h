#pragma once

#include "Support/Enum.h"
#include "Support/Format.h"
#include "Support/GlobPattern.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/Allocator.h"
#include <expected>

namespace clice {

/// A simple string pool to hold all cstring and cstring list.
class StringPool {
public:
    bool contains(llvm::StringRef str) const {
        return pooled_strs.contains(str);
    }

    bool contains(llvm::ArrayRef<const char*> list) const {
        return pooled_str_lists.contains(list);
    }

    /// Save a cstring in the pool, make sure end with `\0`.
    auto save_cstr(llvm::StringRef str) -> llvm::StringRef;

    /// Save a cstring list in the pool.
    auto save_cstr_list(llvm::ArrayRef<const char*> list) -> llvm::ArrayRef<const char*>;

    /// Clear all cached strings.
    void clear();

private:
    /// The memory pool to hold all cstring and cstring list.
    llvm::BumpPtrAllocator allocator;

    /// Cache between input string and its cache cstring in the allocator, make sure end with `\0`.
    llvm::DenseSet<llvm::StringRef> pooled_strs;

    /// Cache between input command and its cache array in the allocator.
    llvm::DenseSet<llvm::ArrayRef<const char*>> pooled_str_lists;
};

/// Refers to clang::driver::options::ID in the `clang::Driver::Options.h`. We use the underlying
/// type directly to avoid including clang headers.
using DriverOptionID = std::uint32_t;

/// See rules in clice.toml for details.
struct Rule {
    /// Whether the file is treated as readonly.
    /// nullopt: auto, true: always, false: never.
    std::optional<bool> readonly;

    /// Whether the file is treated as header file.
    /// nullopt: auto, true: always, false: never.
    std::optional<bool> header;

    /// The glob patterns to match the file path.
    llvm::SmallVector<GlobPattern> pattern;

    /// Additional arguments to append to the command.
    llvm::SmallVector<DriverOptionID> append;

    /// Arguments to remove from the command.
    llvm::DenseMap<DriverOptionID, int> remove;

    /// extra header contexts (file paths) for the file.
    std::vector<std::string> context;
};

namespace config {
struct Rule;
}

class RuleManager {
public:
    /// Clear all rules.
    void clear();

    /// Find the first matched rule for the given file path.
    /// Return nullptr if no rule matches.
    const Rule* find_rule(llvm::StringRef file) const;

    std::pair<bool, const Rule*> try_apply() const;

    static auto load_rules(llvm::ArrayRef<config::Rule> rules) -> RuleManager;

private:
    /// All rules in the order of appearance.
    std::vector<Rule> rules;

    llvm::DenseMap<DriverOptionID, llvm::StringRef> option_id_to_name;
};

struct CommandOptions {
    /// Attach resource directory to the command.
    bool resource_dir = false;

    /// Query the compiler driver for additional information, such as system includes and target.
    bool query_driver = false;

    /// Suppress the warning log if failed to query driver info.
    /// Set true in unittests to avoid cluttering test output.
    bool suppress_log = false;
};

class CompilationDatabase {
public:
    using Self = CompilationDatabase;

    enum class UpdateKind : std::uint8_t {
        Unchange,
        Create,
        Update,
        Delete,
    };

    struct CommandInfo {
        /// TODO: add sysroot or no stdinc command info.
        llvm::StringRef directory;

        /// The canonical command list.
        llvm::ArrayRef<const char*> arguments;
    };

    struct DriverInfo {
        /// The target of this driver.
        llvm::StringRef target;

        /// The default system includes of this driver.
        llvm::ArrayRef<const char*> system_includes;
    };

    struct UpdateInfo {
        /// The kind of update.
        UpdateKind kind;

        llvm::StringRef file;
    };

    struct LookupInfo {
        llvm::StringRef directory;

        std::vector<const char*> arguments;
    };

    struct QueryDriverError {
        struct ErrorKind : refl::Enum<ErrorKind> {
            enum Kind : std::uint8_t {
                NotFoundInPATH,
                FailToCreateTempFile,
                InvokeDriverFail,
                OutputFileNotReadable,
                InvalidOutputFormat,
            };

            using Enum::Enum;
        };

        ErrorKind kind;
        std::string detail;
    };

    CompilationDatabase();

    /// Get an the option for specific argument.
    static std::optional<std::uint32_t> get_option_id(llvm::StringRef argument);

    /// Get inner string pool.
    auto get_string_pool(this Self& self) -> StringPool& {
        return self.pool;
    }

    /// Query the compiler driver and return its driver info.
    auto query_driver(this Self& self, llvm::StringRef driver)
        -> std::expected<DriverInfo, QueryDriverError>;

    /// Update with arguments.
    auto update_command(this Self& self,
                        llvm::StringRef dictionary,
                        llvm::StringRef file,
                        llvm::ArrayRef<const char*> arguments) -> UpdateInfo;

    /// Update with full command.
    auto update_command(this Self& self,
                        llvm::StringRef dictionary,
                        llvm::StringRef file,
                        llvm::StringRef command) -> UpdateInfo;

    /// Update commands from json file and return all updated file.
    auto load_commands(this Self& self, llvm::StringRef json_content, llvm::StringRef workspace)
        -> std::expected<std::vector<UpdateInfo>, std::string>;

    /// Get compile command from database. `file` should has relative path of workspace.
    auto get_command(this Self& self, llvm::StringRef file, CommandOptions options = {})
        -> LookupInfo;

    /// Load compile commands from given directories. If no valid commands are found,
    /// search recursively from the workspace directory.
    auto load_compile_database(this Self& self,
                               llvm::ArrayRef<std::string> compile_commands_dirs,
                               llvm::StringRef workspace) -> void;

    /// Clear all cached commands and drivers. This will not clear the filtered options.
    void clear();

private:
    /// If file not found in CDB file, try to guess commands or use the default case.
    auto guess_or_fallback(this Self& self, llvm::StringRef file) -> LookupInfo;

private:
    /// Cache compilation arguments and system includes.
    StringPool pool;

    /// A map between file path and its canonical command list.
    llvm::DenseMap<const char*, CommandInfo> command_infos;

    /// A map between driver path and its query driver info.
    llvm::DenseMap<const char*, DriverInfo> driver_infos;

    /// The clang options we want to filter in all cases, like -c and -o.
    llvm::DenseSet<DriverOptionID> filtered_options;
};

}  // namespace clice

template <>
struct std::formatter<clice::CompilationDatabase::QueryDriverError> :
    std::formatter<llvm::StringRef> {

    template <typename FormatContext>
    auto format(clice::CompilationDatabase::QueryDriverError& e, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "{} {}", e.kind.name(), e.detail);
    }
};

namespace llvm {

template <>
struct DenseMapInfo<llvm::ArrayRef<const char*>> {
    using T = llvm::ArrayRef<const char*>;

    inline static T getEmptyKey() {
        return T(reinterpret_cast<T::const_pointer>(~0), T::size_type(0));
    }

    inline static T getTombstoneKey() {
        return T(reinterpret_cast<T::const_pointer>(~1), T::size_type(0));
    }

    static unsigned getHashValue(const T& value) {
        return llvm::hash_combine_range(value.begin(), value.end());
    }

    static bool isEqual(const T& lhs, const T& rhs) {
        return lhs == rhs;
    }
};

}  // namespace llvm

