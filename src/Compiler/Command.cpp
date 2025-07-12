#include "Support/Logger.h"
#include "Compiler/Command.h"
#include "Compiler/Compilation.h"
#include "Support/FileSystem.h"
#include "llvm/Support/CommandLine.h"
#include "clang/Driver/Driver.h"

namespace clice {

/// Determine total number of args consumed by this option.
// Return answers for {Exact, Prefix} match. 0 means not allowed.
std::pair<unsigned, unsigned> get_arg_count(const llvm::opt::Option& Opt) {
    constexpr static unsigned Rest = 10000;  // Should be all the rest!
    // Reference is llvm::opt::Option::acceptInternal()
    using llvm::opt::Option;
    switch(Opt.getKind()) {
        case Option::FlagClass: return {1, 0};
        case Option::JoinedClass:
        case Option::CommaJoinedClass: return {1, 1};
        case Option::GroupClass:
        case Option::InputClass:
        case Option::UnknownClass:
        case Option::ValuesClass: return {1, 0};
        case Option::JoinedAndSeparateClass: return {2, 2};
        case Option::SeparateClass: return {2, 0};
        case Option::MultiArgClass: return {1 + Opt.getNumArgs(), 0};
        case Option::JoinedOrSeparateClass: return {2, 1};
        case Option::RemainingArgsClass: return {Rest, 0};
        case Option::RemainingArgsJoinedClass: return {Rest, Rest};
    }
    llvm_unreachable("Unhandled option kind");
}

// Flag-parsing mode, which affects which flags are available.
enum DriverMode : unsigned char {
    DM_None = 0,
    DM_GCC = 1,  // Default mode e.g. when invoked as 'clang'
    DM_CL = 2,   // MS CL.exe compatible mode e.g. when invoked as 'clang-cl'
    DM_CC1 = 4,  // When invoked as 'clang -cc1' or after '-Xclang'
    DM_All = 7
};

// Examine args list to determine if we're in GCC, CL-compatible, or cc1 mode.
DriverMode get_driver_mode(const std::vector<const char*>& Args) {
    DriverMode Mode = DM_GCC;
    llvm::StringRef Argv0 = Args.front();
    if(Argv0.ends_with_insensitive(".exe"))
        Argv0 = Argv0.drop_back(strlen(".exe"));
    if(Argv0.ends_with_insensitive("cl"))
        Mode = DM_CL;
    for(const llvm::StringRef Arg: Args) {
        if(Arg == "--driver-mode=cl") {
            Mode = DM_CL;
            break;
        }
        if(Arg == "-cc1") {
            Mode = DM_CC1;
            break;
        }
    }
    return Mode;
}

unsigned char get_modes(const llvm::opt::Option& Opt) {
    unsigned char Result = DM_None;
    if(Opt.hasVisibilityFlag(clang::driver::options::ClangOption))
        Result |= DM_GCC;
    if(Opt.hasVisibilityFlag(clang::driver::options::CC1Option))
        Result |= DM_CC1;
    if(Opt.hasVisibilityFlag(clang::driver::options::CLOption))
        Result |= DM_CL;
    return Result;
}

llvm::ArrayRef<ArgStripper::Rule> ArgStripper::rulesFor(llvm::StringRef Arg) {
    // All the hard work is done once in a static initializer.
    // We compute a table containing strings to look for and #args to skip.
    // e.g. "-x" => {-x 2 args, -x* 1 arg, --language 2 args, --language=* 1 arg}
    using TableTy = llvm::StringMap<llvm::SmallVector<Rule, 4>, llvm::BumpPtrAllocator>;
    static TableTy* table = [] {
        auto& DriverTable = clang::driver::getDriverOptTable();
        using DriverID = clang::driver::options::ID;

        // Collect sets of aliases, so we can treat -foo and -foo= as synonyms.
        // Conceptually a double-linked list: PrevAlias[I] -> I -> NextAlias[I].
        // If PrevAlias[I] is INVALID, then I is canonical.
        DriverID prev_alias[DriverID::LastOption] = {DriverID::OPT_INVALID};
        DriverID next_alias[DriverID::LastOption] = {DriverID::OPT_INVALID};
        auto add_alias = [&](DriverID self, DriverID other) {
            if(next_alias[other]) {
                prev_alias[next_alias[other]] = self;
                next_alias[self] = next_alias[other];
            }
            prev_alias[self] = other;
            next_alias[other] = self;
        };

        struct {
            DriverID id;
            DriverID alias_id;
            const void* alias_args;
        } alias_table[] = {
#define OPTION(PREFIX,                                                                             \
               PREFIXED_NAME,                                                                      \
               ID,                                                                                 \
               KIND,                                                                               \
               GROUP,                                                                              \
               ALIAS,                                                                              \
               ALIASARGS,                                                                          \
               FLAGS,                                                                              \
               VISIBILITY,                                                                         \
               PARAM,                                                                              \
               HELPTEXT,                                                                           \
               HELPTEXTSFORVARIANTS,                                                               \
               METAVAR,                                                                            \
               VALUES)                                                                             \
    {DriverID::OPT_##ID, DriverID::OPT_##ALIAS, ALIASARGS},
#include "clang/Driver/Options.inc"
#undef OPTION
        };
        for(auto& item: alias_table) {
            if(item.alias_id != DriverID::OPT_INVALID && item.alias_args == nullptr) {
                add_alias(item.id, item.alias_id);
            }
        }

        auto result = std::make_unique<TableTy>();

        // Iterate over distinct options (represented by the canonical alias).
        // Every spelling of this option will get the same set of rules.
        for(unsigned ID = 1 /*Skip INVALID */; ID < DriverID::LastOption; ++ID) {
            if(prev_alias[ID] || ID == DriverID::OPT_Xclang) {
                // Not canonical, or specially handled.
                continue;
            }

            llvm::SmallVector<Rule> rules;

            // Iterate over each alias, to add rules for parsing it.
            for(unsigned A = ID; A != DriverID::OPT_INVALID; A = next_alias[A]) {
                llvm::SmallVector<llvm::StringRef, 4> prefixes;
                DriverTable.appendOptionPrefixes(A, prefixes);
                if(prefixes.empty()) {
                    // option groups.
                    continue;
                }

                auto option = DriverTable.getOption(A);
                // Exclude - and -foo pseudo-options.
                if(option.getName().empty()) {
                    continue;
                }

                auto modes = get_modes(option);
                std::pair<unsigned, unsigned> arg_count = get_arg_count(option);
                // Iterate over each spelling of the alias, e.g. -foo vs --foo.
                for(llvm::StringRef prefix: prefixes) {
                    llvm::SmallString<64> buffer(prefix);
                    buffer.append(option.getName());
                    llvm::StringRef spelling = result->try_emplace(buffer).first->getKey();
                    rules.emplace_back();
                    Rule& rule = rules.back();
                    rule.text = spelling;
                    rule.modes = modes;
                    rule.exact_args = arg_count.first;
                    rule.prefix_args = arg_count.second;
                    // Concrete priority is the index into the option table.
                    // Effectively, earlier entries take priority over later ones.
                    assert(ID < std::numeric_limits<decltype(rule.priority)>::max() &&
                           "Rules::Priority overflowed by options table");
                    rule.priority = ID;
                }
            }

            // Register the set of rules under each possible name.
            for(const auto& R: rules) {
                result->find(R.text)->second.append(rules.begin(), rules.end());
            }
        }

        // The static table will never be destroyed.
        return result.release();
    }();

    auto It = table->find(Arg);
    return (It == table->end()) ? llvm::ArrayRef<Rule>() : It->second;
}

void ArgStripper::strip(llvm::StringRef Arg) {
    auto option_rules = rulesFor(Arg);
    if(option_rules.empty()) {
        // Not a recognized flag. Strip it literally.
        storage.emplace_back(Arg);
        rules.emplace_back();
        rules.back().text = storage.back();
        rules.back().exact_args = 1;
        if(rules.back().text.consume_back("*")) {
            rules.back().prefix_args = 1;
        }

        rules.back().modes = DM_All;
        rules.back().priority = -1;  // Max unsigned = lowest priority.
    } else {
        rules.append(option_rules.begin(), option_rules.end());
    }
}

const ArgStripper::Rule* ArgStripper::matching_rule(llvm::StringRef arg,
                                                    unsigned Mode,
                                                    unsigned& arg_count) const {
    const ArgStripper::Rule* best_rule = nullptr;
    for(const Rule& rule: rules) {
        // Rule can fail to match if...
        if(!(rule.modes & Mode)) {
            // not applicable to current driver mode
            continue;
        }

        if(best_rule && best_rule->priority < rule.priority) {
            // lower-priority than best candidate.
            continue;
        }

        if(!arg.starts_with(rule.text)) {
            // current arg doesn't match the prefix string
            continue;
        }

        bool prefix_match = arg.size() > rule.text.size();

        // Can rule apply as an exact/prefix match?
        if(unsigned count = prefix_match ? rule.prefix_args : rule.exact_args) {
            best_rule = &rule;
            arg_count = count;
        }
        // Continue in case we find a higher-priority rule.
    }
    return best_rule;
}

void ArgStripper::process(std::vector<const char*>& args) const {
    if(args.empty())
        return;

    // We're parsing the args list in some mode (e.g. gcc-compatible) but may
    // temporarily switch to another mode with the -Xclang flag.
    DriverMode main_mode = get_driver_mode(args);
    DriverMode current_mode = main_mode;

    // Read and write heads for in-place deletion.
    std::uint32_t read = 0;
    std::uint32_t write = 0;
    bool was_xclang = false;

    while(read < args.size()) {
        unsigned arg_count = 0;
        if(matching_rule(args[read], current_mode, arg_count)) {
            // Delete it and its args.
            if(was_xclang) {
                assert(write > 0);
                write -= 1;  // Drop previous -Xclang arg
                current_mode = main_mode;
                was_xclang = false;
            }

            // Advance to last arg. An arg may be foo or -Xclang foo.
            for(unsigned I = 1; read < args.size() && I < arg_count; ++I) {
                read += 1;
                if(read < args.size() && args[read] == std::string_view("-Xclang")) {
                    read += 1;
                }
            }
        } else {
            // No match, just copy the arg through.
            was_xclang = args[read] == std::string_view("-Xclang");
            current_mode = was_xclang ? DM_CC1 : main_mode;
            if(write != read) {
                args[write] = std::move(args[read]);
            }
            write += 1;
        }
        read += 1;
    }

    args.resize(write);
}

CompilationDatabase::CompilationDatabase() {
    resource_dir_opt = save_string(std::format("-resource-dir={}", fs::resource_dir));

    /// Add arguments that we want to remove.
    stripper.strip("-o");
    stripper.strip("-c");
}

void CompilationDatabase::update_commands(this Self& self, llvm::StringRef filename) {
    auto path = path::real_path(filename);
    filename = path;

    /// Read the compile commands from the file.
    json::Value json = nullptr;

    if(auto buffer = llvm::MemoryBuffer::getFile(filename)) {
        if(auto result = json::parse(buffer->get()->getBuffer())) {
            /// llvm::json::Value will hold on string buffer.
            /// Do not worry about the lifetime of the buffer.
            /// Release buffer to save memory.
            json = std::move(result.get());
        } else {
            log::warn("Failed to parse json file at {0}, because {1}",
                      filename,
                      result.takeError());
            return;
        }
    } else {
        log::warn("Failed to read file {0}", filename);
        return;
    }

    assert(json.kind() != json::Value::Null && "json is nullptr");

    if(json.kind() != json::Value::Array) {
        log::warn(
            "Compilation CompilationDatabase requires a array of object, but get {0}, input file: {1}",
            refl::enum_name(json.kind()),
            filename);
        return;
    }

    auto elements = json.getAsArray();
    assert(elements && "json is not an array");

    for(auto& element: *elements) {
        auto object = element.getAsObject();
        if(!object) {
            log::warn(
                "Compilation CompilationDatabase requires an array of object, but get a array of {0}, input file: {1}",
                refl::enum_name(element.kind()),
                filename);
            continue;
        }

        /// FIXME: currently we assume all path here is absolute.
        /// Add `directory` field in the future.

        llvm::SmallString<128> path;

        if(auto file = object->getString("file")) {
            if(auto error = fs::real_path(*file, path)) {
                log::warn("Failed to get real path of {0}, because {1}", *file, error.message());
                continue;
            }
        } else {
            log::warn("The element does not have a file field, input file: {0}", filename);
            continue;
        }

        auto command = object->getString("command");
        if(!command) {
            log::warn("The key:{0} does not have a command field, input file: {1}", path, filename);
            continue;
        }

        self.add_command(path, *command);
    }

    log::info("Successfully loaded compile commands from {0}, total {1} commands",
              filename,
              self.commands.size());

    /// Scan all files to build module map.
    // CompilationParams params;
    // for(auto& [path, command]: commands) {
    //     params.srcPath = path;
    //     params.command = command;
    //
    //    auto name = scanModuleName(params);
    //    if(!name.empty()) {
    //        moduleMap[name] = path;
    //    }
    //}

    log::info("Successfully built module map, total {0} modules", self.moduleMap.size());
}

void CompilationDatabase::update_module(llvm::StringRef file, llvm::StringRef name) {
    moduleMap[path::real_path(file)] = file;
}

llvm::StringRef CompilationDatabase::get_module_file(llvm::StringRef name) {
    auto iter = moduleMap.find(name);
    if(iter == moduleMap.end()) {
        return "";
    }
    return iter->second;
}

llvm::StringRef CompilationDatabase::save_string(this Self& self, llvm::StringRef string) {
    auto it = self.unique.find(string);

    /// FIXME: arg may be empty?

    /// If we already store the argument, reuse it.
    if(it != self.unique.end()) {
        return *it;
    }

    /// Allocate new argument.
    const auto size = string.size();
    auto ptr = self.memory_pool.Allocate<char>(size + 1);
    std::memcpy(ptr, string.data(), size);
    ptr[size] = '\0';

    /// Insert new argument.
    auto result = llvm::StringRef(ptr, size);
    self.unique.insert(result);
    return result;
}

std::vector<const char*> CompilationDatabase::save_args(this Self& self,
                                                        llvm::ArrayRef<const char*> args) {
    std::vector<const char*> result;
    result.reserve(args.size());

    for(auto i = 0; i < args.size(); i++) {
        result.emplace_back(self.save_string(args[i]).data());
    }

    return result;
}

void CompilationDatabase::add_command(this Self& self,
                                      llvm::StringRef path,
                                      llvm::StringRef command,
                                      Style style) {
    llvm::SmallVector<const char*> args;

    /// temporary allocator to meet the argument requirements of tokenize.
    llvm::BumpPtrAllocator allocator;
    llvm::StringSaver saver(allocator);

    /// FIXME: we may want to check the first argument of command to
    /// make sure its mode.
    if(style == Style::GNU) {
        llvm::cl::TokenizeGNUCommandLine(command, saver, args);
    } else if(style == Style::MSVC) {
        llvm::cl::TokenizeWindowsCommandLineFull(command, saver, args);
    } else {
        std::abort();
    }

    auto path_ = self.save_string(path);
    auto new_args = self.save_args(args);

    /// FIXME: Use a better way to handle resource dir.
    /// new_args.push_back(self.save_string(std::format("-resource-dir={}",
    /// fs::resource_dir)).data());

    self.stripper.process(new_args);

    auto it = self.commands.find(path_.data());
    if(it == self.commands.end()) {
        self.commands.try_emplace(path_.data(),
                                  std::make_unique<std::vector<const char*>>(std::move(new_args)));
    } else {
        *it->second = std::move(new_args);
    }
}

std::vector<const char*> CompilationDatabase::get_command(this Self& self,
                                                          llvm::StringRef path,
                                                          bool query_driver,
                                                          bool append_resource_dir) {
    std::vector<const char*> result;
    auto path_ = self.save_string(path);
    auto it = self.commands.find(path_.data());
    if(it != self.commands.end()) {
        result = *it->second;
    }

    if(append_resource_dir) {
        result.emplace_back(self.resource_dir_opt.data());
    }

    /// TODO: query driver.

    return result;
}

}  // namespace clice
