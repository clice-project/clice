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
        llvm::StringRef dictionary;

        std::vector<const char*> arguments;
    };

    CompilationDatabase();

    auto save_string(this Self& self, llvm::StringRef string) -> llvm::StringRef;

    auto save_cstring_list(this Self& self, llvm::ArrayRef<const char*> arguments)
        -> llvm::ArrayRef<const char*>;

    /// Get an the option for specific argument.
    static std::optional<std::uint32_t> get_option_id(llvm::StringRef argument);

    /// Query the compiler driver and return its driver info.
    auto query_driver(this Self& self, llvm::StringRef driver)
        -> std::expected<DriverInfo, std::string>;

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

    auto get_command(this Self& self,
                     llvm::StringRef file,
                     bool resource_dir = false,
                     bool query_driver = false) -> LookupInfo;

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
