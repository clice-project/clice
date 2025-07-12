#pragma once

#include <expected>
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/Allocator.h"
#include <deque>

namespace clice {

// Removes args from a command-line in a semantically-aware way.
//
// Internally this builds a large (0.5MB) table of clang options on first use.
// Both strip() and process() are fairly cheap after that.
//
// FIXME: this reimplements much of OptTable, it might be nice to expose more.
// The table-building strategy may not make sense outside clangd.
class ArgStripper {
public:
    ArgStripper() = default;
    ArgStripper(ArgStripper&&) = default;
    ArgStripper(const ArgStripper&) = delete;
    ArgStripper& operator= (ArgStripper&&) = default;
    ArgStripper& operator= (const ArgStripper&) = delete;

    // Adds the arg to the set which should be removed.
    //
    // Recognized clang flags are stripped semantically. When "-I" is stripped:
    //  - so is its value (either as -Ifoo or -I foo)
    //  - aliases like --include-directory=foo are also stripped
    //  - CL-style /Ifoo will be removed if the args indicate MS-compatible mode
    // Compile args not recognized as flags are removed literally, except:
    //  - strip("ABC*") will remove any arg with an ABC prefix.
    //
    // In either case, the -Xclang prefix will be dropped if present.
    void strip(llvm::StringRef arg);
    // Remove the targets from a compile command, in-place.
    void process(std::vector<const char*>& args) const;

private:
    // Deletion rules, to be checked for each arg.
    struct Rule {
        llvm::StringRef text;      // Rule applies only if arg begins with Text.
        unsigned char modes = 0;   // Rule applies only in specified driver modes.
        uint16_t priority = 0;     // Lower is better.
        uint16_t exact_args = 0;   // Num args consumed when Arg == Text.
        uint16_t prefix_args = 0;  // Num args consumed when Arg starts with Text.
    };

    static llvm::ArrayRef<Rule> rulesFor(llvm::StringRef arg);
    const Rule* matching_rule(llvm::StringRef arg, unsigned mode, unsigned& arg_count) const;
    llvm::SmallVector<Rule> rules;
    std::deque<std::string> storage;  // Store strings not found in option table.
};

/// `CompilationDatabase` is responsible for managing the compile commands.
///
/// FIXME: currently we assume that a file only occurs once in the CDB.
/// This is not always correct, but it is enough for now.
class CompilationDatabase {
public:
    using Self = CompilationDatabase;

    CompilationDatabase();

    /// Update the compile commands with the given file.
    void update_commands(this Self& self, llvm::StringRef file);

    /// Update the module map with the given file and module name.
    void update_module(llvm::StringRef file, llvm::StringRef name);

    /// Lookup the module interface unit file path of the given module name.
    llvm::StringRef get_module_file(llvm::StringRef name);

    enum class Style {
        GNU = 0,
        MSVC,
    };

    void add_command(this Self& self,
                     llvm::StringRef path,
                     llvm::StringRef command,
                     Style style = Style::GNU);

    std::vector<const char*> get_command(this Self& self,
                                         llvm::StringRef path,
                                         bool query_driver = false,
                                         bool append_resource_dir = false);

    struct Rule {};

private:
    /// Save a string into memory pool. Make sure end with `\0`.
    llvm::StringRef save_string(this Self& self, llvm::StringRef string);

    std::vector<const char*> save_args(this Self& self, llvm::ArrayRef<const char*> args);

private:
    ArgStripper stripper;

    /// For C++20 module, we only can got dependent module name
    /// in source context. But we need dependent module file path
    /// to build PCM. So we will scan(preprocess) all project files
    /// to build a module map between module name and module file path.
    /// **Note that** this only includes module interface unit, for module
    /// implementation unit, the scan could be delayed until compiling it.
    llvm::StringMap<std::string> moduleMap;

    /// An opt to add resource dir, like `-resource-dir=xxx`.
    llvm::StringRef resource_dir_opt;

    /// Memory pool for command arguments.
    llvm::BumpPtrAllocator memory_pool;

    /// For lookup whether we already have the key.
    llvm::DenseSet<llvm::StringRef> unique;

    // A map between file path and compile commands.
    /// TODO: Path cannot represent unique file, we should use better, like inode ...
    llvm::DenseMap<const char*, std::unique_ptr<std::vector<const char*>>> commands;
};

}  // namespace clice

