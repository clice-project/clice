#include "Support/Logger.h"
#include "Compiler/Command.h"
#include "Compiler/Compilation.h"
#include "Support/FileSystem.h"
#include "llvm/Support/CommandLine.h"
#include "clang/Driver/Driver.h"

namespace clice {

auto CompilationDatabase::save_string(this Self& self, llvm::StringRef string) -> llvm::StringRef {
    assert(!string.empty() && "expected non empty string");
    auto it = self.string_cache.find(string);

    /// If we already store the argument, reuse it.
    if(it != self.string_cache.end()) {
        return *it;
    }

    /// Allocate for new string.
    const auto size = string.size();
    auto ptr = self.allocator.Allocate<char>(size + 1);
    std::memcpy(ptr, string.data(), size);
    ptr[size] = '\0';

    /// Insert it to cache.
    auto result = llvm::StringRef(ptr, size);
    self.string_cache.insert(result);
    return result;
}

auto CompilationDatabase::save_arguments(this Self& self, llvm::ArrayRef<const char*> arguments)
    -> llvm::ArrayRef<const char*> {
    auto it = self.arguments_cache.find(arguments);

    /// If we already store the argument, reuse it.
    if(it != self.arguments_cache.end()) {
        return *it;
    }

    /// Allocate for new array.
    const auto size = arguments.size();
    auto ptr = self.allocator.Allocate<const char*>(size);
    ranges::copy(arguments, ptr);

    /// Insert it to cache.
    auto result = llvm::ArrayRef<const char*>(ptr, size);
    self.arguments_cache.insert(result);
    return result;
}

CompilationDatabase::CompilationDatabase() {
    using opions = clang::driver::options::ID;

    /// Remove the input file, we will add input file ourselves.
    filtered_options.insert(opions::OPT_INPUT);

    /// -c and -o are meaningless for frontend.
    filtered_options.insert(opions::OPT_c);
    filtered_options.insert(opions::OPT_o);
    filtered_options.insert(opions::OPT_dxc_Fc);
    filtered_options.insert(opions::OPT_dxc_Fo);

    /// Remove all options related to PCH building.
    filtered_options.insert(opions::OPT_emit_pch);
    filtered_options.insert(opions::OPT_include_pch);
    filtered_options.insert(opions::OPT__SLASH_Yu);
    filtered_options.insert(opions::OPT__SLASH_Fp);

    /// Remove all options related to C++ module, we will
    /// build module and set deps ourselves.
    filtered_options.insert(opions::OPT_fmodule_file);
    filtered_options.insert(opions::OPT_fmodule_output);
    filtered_options.insert(opions::OPT_fprebuilt_module_path);
}

std::optional<std::uint32_t> CompilationDatabase::get_option_id(llvm::StringRef argument) {
    auto& table = clang::driver::getDriverOptTable();

    llvm::SmallString<64> buffer = argument;

    if(argument.ends_with("=")) {
        buffer += "placeholder";
    }

    unsigned index = 0;
    std::array arguments = {buffer.c_str(), "placeholder"};
    llvm::opt::InputArgList arg_list(arguments.begin(), arguments.end());

    if(auto arg = table.ParseOneArg(arg_list, index)) {
        return arg->getOption().getID();
    } else {
        return {};
    }
}

void CompilationDatabase::add_filter(this Self& self, std::uint32_t id) {
    self.filtered_options.insert(id);
}

void CompilationDatabase::add_filter(this Self& self, llvm::StringRef arg) {
    auto& table = clang::driver::getDriverOptTable();

    llvm::SmallString<64> buffer = arg;

    if(arg.ends_with("=")) {
        buffer += "placeholder";
    }

    unsigned index = 0;
    std::array arguments = {buffer.c_str(), "placeholder"};
    llvm::opt::InputArgList arg_list(arguments.begin(), arguments.end());

    if(auto arg = table.ParseOneArg(arg_list, index)) {
        self.filtered_options.insert(arg->getOption().getID());
    } else {
        llvm::errs() << "Fail to parse argument" << "\n";
    }
}

auto CompilationDatabase::update_command(this Self& self,
                                          llvm::StringRef dictionary,
                                          llvm::StringRef file,
                                          llvm::ArrayRef<const char*> arguments) -> UpdateInfo {
    file = self.save_string(file);
    dictionary = self.save_string(dictionary);

    llvm::SmallVector<const char*, 16> filtered_arguments;

    /// Append
    auto add_argument = [&](llvm::StringRef argument) {
        auto saved = self.save_string(argument);
        filtered_arguments.emplace_back(saved.data());
    };

    /// Append driver sperately.
    add_argument(arguments.front());

    unsigned missing_arg_index = 0;
    unsigned missing_arg_count = 0;
    auto& table = clang::driver::getDriverOptTable();

    /// The driver should be discarded.
    auto list = table.ParseArgs(arguments.drop_front(), missing_arg_index, missing_arg_count);

    bool remove_pch = false;

    /// Append and filter useless arguments.
    for(auto arg: list.getArgs()) {
        auto& opt = arg->getOption();
        auto id = opt.getID();

        /// Filter options we don't need.
        if(self.filtered_options.contains(id)) {
            continue;
        }

        /// A workaround to remove extra PCH when cmake
        /// generate PCH flags for clang.
        if(id == clang::driver::options::OPT_Xclang) {
            if(arg->getNumValues() == 1) {
                if(remove_pch) {
                    remove_pch = false;
                    continue;
                }

                llvm::StringRef value = arg->getValue(0);
                if(value == "-include-pch") {
                    remove_pch = true;
                    continue;
                }
            }
        }

        /// Rewrite the argument to filter arguments, we basically reimplement
        /// the logic of `Arg::render` to use our allocator to allocate memory.
        switch(opt.getRenderStyle()) {
            case llvm::opt::Option::RenderValuesStyle: {
                for(auto value: arg->getValues()) {
                    add_argument(value);
                }
                break;
            }

            case llvm::opt::Option::RenderSeparateStyle: {
                add_argument(arg->getSpelling());
                for(auto value: arg->getValues()) {
                    add_argument(value);
                }
                break;
            }

            case llvm::opt::Option::RenderJoinedStyle: {
                llvm::SmallString<256> first = {arg->getSpelling(), arg->getValue(0)};
                add_argument(first);
                for(auto value: llvm::ArrayRef(arg->getValues()).drop_front()) {
                    add_argument(value);
                }
                break;
            }

            case llvm::opt::Option::RenderCommaJoinedStyle: {
                llvm::SmallString<256> buffer = arg->getSpelling();
                for(auto i = 0; i < arg->getNumValues(); i++) {
                    if(i) {
                        buffer += ',';
                    }
                    buffer += arg->getValue(i);
                }
                add_argument(buffer);
                break;
            }
        }
    }

    /// Save arguments.
    arguments = self.save_arguments(filtered_arguments);

    UpdateKind kind = UpdateKind::Unchange;
    CommandInfo info = {dictionary, arguments};
    auto [it, success] = self.command_infos.try_emplace(file.data(), info);
    if(success) {
        kind = UpdateKind::Create;
    } else {
        auto& info = it->second;
        if(info.dictionary.data() != dictionary.data() ||
           info.arguments.data() != arguments.data()) {
            kind = UpdateKind::Update;
            info.dictionary = dictionary;
            info.arguments = arguments;
        }
    }

    return UpdateInfo{kind, file, info};
}

auto CompilationDatabase::update_command(this Self& self,
                                          llvm::StringRef dictionary,
                                          llvm::StringRef file,
                                          llvm::StringRef command) -> UpdateInfo {
    llvm::BumpPtrAllocator local;
    llvm::StringSaver saver(local);

    llvm::SmallVector<const char*, 32> arguments;
    auto [driver, _] = command.split(' ');
    driver = path::filename(driver);

    /// FIXME: Use a better to handle this.
    if(driver.starts_with("cl") || driver.starts_with("clang-cl")) {
        llvm::cl::TokenizeWindowsCommandLineFull(command, saver, arguments);
    } else {
        llvm::cl::TokenizeGNUCommandLine(command, saver, arguments);
    }

    return self.update_command(dictionary, file, arguments);
}

auto CompilationDatabase::load_commands(this Self& self, llvm::StringRef json_content)
    -> std::expected<std::vector<UpdateInfo>, std::string> {
    std::vector<UpdateInfo> infos;

    auto json = json::parse(json_content);
    if(!json) {
        return std::unexpected(std::format("Fail to parse json: {}", json.takeError()));
    }

    if(json->kind() != json::Value::Array) {
        return std::unexpected("Compilation Database must be an array of object");
    }

    /// FIXME: warn illegal item.
    for(auto& item: *json->getAsArray()) {
        /// Ignore non-object item.
        if(item.kind() != json::Value::Object) {
            continue;
        }

        auto& object = *item.getAsObject();

        auto file = object.getString("file");
        auto directory = object.getString("directory");
        if(!file || !directory) {
            continue;
        }

        if(auto arguments = object.getArray("arguments")) {
            /// Construct cstring array.
            llvm::BumpPtrAllocator local;
            llvm::StringSaver saver(local);
            llvm::SmallVector<const char*, 32> carguments;

            for(auto& argument: *arguments) {
                if(argument.kind() == json::Value::String) {
                    carguments.emplace_back(saver.save(*argument.getAsString()).data());
                }
            }

            auto info = self.update_command(*directory, *file, carguments);
            if(info.kind != UpdateKind::Unchange) {
                infos.emplace_back(info);
            }
        } else if(auto command = object.getString("command")) {
            auto info = self.update_command(*directory, *file, *command);
            if(info.kind != UpdateKind::Unchange) {
                infos.emplace_back(info);
            }
        }
    }

    return infos;
}

auto CompilationDatabase::get_command(this Self& self, llvm::StringRef file) -> LookupInfo {
    LookupInfo info;

    file = self.save_string(file);
    auto it = self.command_infos.find(file.data());
    if(it != self.command_infos.end()) {
        info.dictionary = it->second.dictionary;
        info.arguments = it->second.arguments;
        info.arguments.emplace_back(file.data());
    }

    return info;
}

}  // namespace clice
