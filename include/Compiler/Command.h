#pragma once

#include <expected>
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/Allocator.h"
#include "llvm/ADT/ArrayRef.h"
#include <deque>

namespace clice {

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
        llvm::StringRef dictionary;

        /// The canonical command list.
        llvm::ArrayRef<const char*> arguments;
    };

    struct DriverInfo {};

    struct UpdateInfo {
        /// The kind of update.
        UpdateKind kind;

        llvm::StringRef file;

        /// The info of updated command.
        CommandInfo cmd_info;
    };

    struct LookupInfo {
        llvm::StringRef dictionary;

        std::vector<const char*> arguments;
    };

private:
    auto save_string(this Self& self, llvm::StringRef string) -> llvm::StringRef;

    auto save_arguments(this Self& self, llvm::ArrayRef<const char*> arguments)
        -> llvm::ArrayRef<const char*>;

public:
    CompilationDatabase();

    /// Get an the option for specific argument.
    static std::optional<std::uint32_t> get_option_id(llvm::StringRef argument);

    void add_filter(this Self& self, std::uint32_t id);

    /// Add a filter.
    void add_filter(this Self& self, llvm::StringRef arg);

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
    auto load_commands(this Self& self, llvm::StringRef json_content)
        -> std::expected<std::vector<UpdateInfo>, std::string>;

    auto get_command(this Self& self, llvm::StringRef file) -> LookupInfo;

private:
    /// The memory pool to hold all cstring and command list.
    llvm::BumpPtrAllocator allocator;

    /// A cache between input string and its cache cstring
    /// in the allocator, make sure end with `\0`.
    llvm::DenseSet<llvm::StringRef> string_cache;

    /// A cache between input command and its cache array
    /// in the allocator.
    llvm::DenseSet<llvm::ArrayRef<const char*>> arguments_cache;

    /// The clang options we want to filter in all cases, like -c and -o.
    llvm::DenseSet<std::uint32_t> filtered_options;

    /// A map between file path and its canonical command list.
    llvm::DenseMap<const void*, CommandInfo> command_infos;

    /// A map between driver path and its query driver info.
    llvm::DenseMap<const void*, DriverInfo> driver_infos;
};

}  // namespace clice

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
