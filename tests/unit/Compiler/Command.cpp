#include "Test/Test.h"
#include "Compiler/Command.h"
#include "clang/Driver/Driver.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Program.h"
#include "llvm/ADT/ScopeExit.h"

namespace clice::testing {

namespace {

std::string printArgv(llvm::ArrayRef<const char*> args) {
    std::string buf;
    llvm::raw_string_ostream os(buf);
    bool Sep = false;
    for(llvm::StringRef arg: args) {
        if(Sep)
            os << ' ';
        Sep = true;
        if(llvm::all_of(arg, llvm::isPrint) &&
           arg.find_first_of(" \t\n\"\\") == llvm::StringRef::npos) {
            os << arg;
            continue;
        }
        os << '"';
        os.write_escaped(arg, /*UseHexEscapes=*/true);
        os << '"';
    }
    return std::move(os.str());
}

void parse_and_dump(llvm::StringRef command) {
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

    auto& table = clang::driver::getDriverOptTable();
    std::uint32_t count = 0;
    std::uint32_t index = 0;
    auto list = table.ParseArgs(arguments, count, index);

    for(auto arg: list.getArgs()) {
        arg->dump();
    }
}

suite<"Command"> command = [] {
    auto expect_strip = [](llvm::StringRef argv, llvm::StringRef result) {
        CompilationDatabase database;
        llvm::StringRef file = "main.cpp";
        database.update_command("fake/", file, argv);

        CommandOptions options;
        options.suppress_log = true;
        expect(that % printArgv(database.get_command(file, options).arguments) == result);
    };

    test("GetOptionID") = [] {
        auto GET_OPTION_ID = CompilationDatabase::get_option_id;
        namespace option = clang::driver::options;

        /// GroupClass
        expect(that % GET_OPTION_ID("-g") == option::OPT_g_Flag);

        /// InputClass
        expect(that % GET_OPTION_ID("main.cpp") == option::OPT_INPUT);

        /// UnknownClass
        expect(that % GET_OPTION_ID("--clice") == option::OPT_UNKNOWN);

        /// FlagClass
        expect(that % GET_OPTION_ID("-v") == option::OPT_v);
        expect(that % GET_OPTION_ID("-c") == option::OPT_c);
        expect(that % GET_OPTION_ID("-pedantic") == option::OPT_pedantic);
        expect(that % GET_OPTION_ID("--pedantic") == option::OPT_pedantic);

        /// JoinedClass
        expect(that % GET_OPTION_ID("-Wno-unused-variable") == option::OPT_W_Joined);
        expect(that % GET_OPTION_ID("-W*") == option::OPT_W_Joined);
        expect(that % GET_OPTION_ID("-W") == option::OPT_W_Joined);

        /// ValuesClass

        /// SeparateClass
        expect(that % GET_OPTION_ID("-Xclang") == option::OPT_Xclang);
        /// expect(that % GET_ID("-Xclang -ast-dump") == option::OPT_Xclang);

        /// RemainingArgsClass

        /// RemainingArgsJoinedClass

        /// CommaJoinedClass
        expect(that % GET_OPTION_ID("-Wl,") == option::OPT_Wl_COMMA);

        /// MultiArgClass

        /// JoinedOrSeparateClass
        expect(that % GET_OPTION_ID("-o") == option::OPT_o);
        expect(that % GET_OPTION_ID("-I") == option::OPT_I);
        expect(that % GET_OPTION_ID("--include-directory=") == option::OPT_I);
        expect(that % GET_OPTION_ID("-x") == option::OPT_x);
        expect(that % GET_OPTION_ID("--language=") == option::OPT_x);

        /// JoinedAndSeparateClass
    };

    test("DefaultFilters") = [&] {
        /// Filter -c, -o and input file.
        expect_strip("g++ main.cc", "g++ main.cpp");
        expect_strip("clang++ -c main.cpp", "clang++ main.cpp");
        expect_strip("clang++ -o main.o main.cpp", "clang++ main.cpp");
        expect_strip("clang++ -c -o main.o main.cc", "clang++ main.cpp");
        expect_strip("cl.exe /c /Fomain.cpp.o main.cpp", "cl.exe main.cpp");

        /// Filter PCH related.

        /// CMake
        expect_strip(
            "g++ -std=gnu++20 -Winvalid-pch -include cmake_pch.hxx -o main.cpp.o -c main.cpp",
            "g++ -std=gnu++20 -Winvalid-pch -include cmake_pch.hxx main.cpp");
        expect_strip(
            "clang++ -Winvalid-pch -Xclang -include-pch -Xclang cmake_pch.hxx.pch -Xclang -include -Xclang cmake_pch.hxx -o main.cpp.o -c main.cpp",
            "clang++ -Winvalid-pch -Xclang -include -Xclang cmake_pch.hxx main.cpp");
        expect_strip("cl.exe /Yufoo.h /FIfoo.h /Fpfoo.h_v143.pch /c /Fomain.cpp.o main.cpp",
                     "cl.exe -include foo.h main.cpp");

        /// TODO: Test more commands from other build system.
    };

    test("Reuse") = [] {
        using namespace std::literals;

        CompilationDatabase database;
        database.update_command("fake", "test.cpp", "clang++ -std=c++23 test.cpp"sv);
        database.update_command("fake", "test2.cpp", "clang++ -std=c++23 test2.cpp"sv);

        CommandOptions options;
        options.suppress_log = true;
        auto command1 = database.get_command("test.cpp", options).arguments;
        auto command2 = database.get_command("test2.cpp", options).arguments;
        expect(that % command1.size() == 3);
        expect(that % command2.size() == 3);

        expect(that % command1[0] == "clang++"sv);
        expect(that % command1[1] == "-std=c++23"sv);
        expect(that % command1[2] == "test.cpp"sv);

        expect(that % command1[0] == command2[0]);
        expect(that % command1[1] == command2[1]);
        expect(that % command2[2] == "test2.cpp"sv);
    };

    test("Module") = [] {
        // Empty test
    };

    test("QueryDriver") = [] {
#ifdef _GLIBCXX_RELEASE
        using namespace std::literals;
        using ErrorKind = CompilationDatabase::QueryDriverError::ErrorKind;

        CompilationDatabase database;
        auto info = database.query_driver("g++");
        if(!info) {
            auto& err = info.error();
            /// If driver not installed or not found in PATH, skip the following test to avoid
            /// failures on developer's machine, but never skip the test in CI.
            if(err.kind == ErrorKind::NotFoundInPATH && !std::getenv("CI")) {
                return;
            }
        }

        expect(that % info);

        expect(that % info->target == llvm::StringRef("x86_64-linux-gnu"));
        expect(that % info->system_includes.size() == 6);

        if(_GLIBCXX_RELEASE == 13) {
            expect(that % info->system_includes[0] == "/usr/include/c++/13"sv);
            expect(that % info->system_includes[1] == "/usr/include/x86_64-linux-gnu/c++/13"sv);
            expect(that % info->system_includes[2] == "/usr/include/c++/13/backward"sv);
        } else if(_GLIBCXX_RELEASE == 14) {
            expect(that % info->system_includes[0] == "/usr/include/c++/14"sv);
            expect(that % info->system_includes[1] == "/usr/include/x86_64-linux-gnu/c++/14"sv);
            expect(that % info->system_includes[2] == "/usr/include/c++/14/backward"sv);
        }

        expect(that % info->system_includes[3] == "/usr/local/include"sv);
        expect(that % info->system_includes[4] == "/usr/include/x86_64-linux-gnu"sv);
        expect(that % info->system_includes[5] == "/usr/include"sv);

        info = database.query_driver("clang++");
        expect(that % info);
#endif
    };

    test("ResourceDir") = [] {
        /// CompilationDatabase database;
        /// database.update_command("test.cpp", "clang++ -std=c++23 test.cpp");
        /// auto command = database.get_command("test.cpp", false, true);
        /// expect(that % command.size() == 4);
        ///
        /// using namespace std::literals;
        /// expect(that % command[0] == "clang++"sv);
        /// expect(that % command[1] == "-std=c++23"sv);
        /// expect(that % command[2] == "test.cpp"sv);
        /// expect(that % command[3] == std::format("-resource-dir={}", fs::resource_dir));
    };
};

}  // namespace

}  // namespace clice::testing
