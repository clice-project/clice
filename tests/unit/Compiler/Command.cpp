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

TEST(Command, GetOptionID) {
    auto GET_OPTION_ID = CompilationDatabase::get_option_id;
    namespace option = clang::driver::options;

    /// GroupClass
    EXPECT_EQ(GET_OPTION_ID("-g"), option::OPT_g_Flag);

    /// InputClass
    EXPECT_EQ(GET_OPTION_ID("main.cpp"), option::OPT_INPUT);

    /// UnknownClass
    EXPECT_EQ(GET_OPTION_ID("--clice"), option::OPT_UNKNOWN);

    /// FlagClass
    EXPECT_EQ(GET_OPTION_ID("-v"), option::OPT_v);
    EXPECT_EQ(GET_OPTION_ID("-c"), option::OPT_c);
    EXPECT_EQ(GET_OPTION_ID("-pedantic"), option::OPT_pedantic);
    EXPECT_EQ(GET_OPTION_ID("--pedantic"), option::OPT_pedantic);

    /// JoinedClass
    EXPECT_EQ(GET_OPTION_ID("-Wno-unused-variable"), option::OPT_W_Joined);
    EXPECT_EQ(GET_OPTION_ID("-W*"), option::OPT_W_Joined);
    EXPECT_EQ(GET_OPTION_ID("-W"), option::OPT_W_Joined);

    /// ValuesClass

    /// SeparateClass
    EXPECT_EQ(GET_OPTION_ID("-Xclang"), option::OPT_Xclang);
    /// EXPECT_EQ(GET_ID("-Xclang -ast-dump"), option::OPT_Xclang);

    /// RemainingArgsClass

    /// RemainingArgsJoinedClass

    /// CommaJoinedClass
    EXPECT_EQ(GET_OPTION_ID("-Wl,"), option::OPT_Wl_COMMA);

    /// MultiArgClass

    /// JoinedOrSeparateClass
    EXPECT_EQ(GET_OPTION_ID("-o"), option::OPT_o);
    EXPECT_EQ(GET_OPTION_ID("-I"), option::OPT_I);
    EXPECT_EQ(GET_OPTION_ID("--include-directory="), option::OPT_I);
    EXPECT_EQ(GET_OPTION_ID("-x"), option::OPT_x);
    EXPECT_EQ(GET_OPTION_ID("--language="), option::OPT_x);

    /// JoinedAndSeparateClass
}

void EXPECT_STRIP(llvm::StringRef argv,
                  llvm::StringRef result,
                  LocationChain chain = LocationChain()) {
    CompilationDatabase database;
    database.update_command("fake/", "main.cpp", argv);
    EXPECT_EQ(printArgv(database.get_command("main.cpp").arguments), result, chain);
};

TEST(Command, DefaultFilters) {
    /// Filter -c, -o and input file.
    EXPECT_STRIP("g++ main.cc", "g++ main.cpp");
    EXPECT_STRIP("clang++ -c main.cpp", "clang++ main.cpp");
    EXPECT_STRIP("clang++ -o main.o main.cpp", "clang++ main.cpp");
    EXPECT_STRIP("clang++ -c -o main.o main.cc", "clang++ main.cpp");
    EXPECT_STRIP("cl.exe /c /Fomain.cpp.o main.cpp", "cl.exe main.cpp");

    /// Filter PCH related.

    /// CMake
    EXPECT_STRIP("g++ -std=gnu++20 -Winvalid-pch -include cmake_pch.hxx -o main.cpp.o -c main.cpp",
                 "g++ -std=gnu++20 -Winvalid-pch -include cmake_pch.hxx main.cpp");
    EXPECT_STRIP(
        "clang++ -Winvalid-pch -Xclang -include-pch -Xclang cmake_pch.hxx.pch -Xclang -include -Xclang cmake_pch.hxx -o main.cpp.o -c main.cpp",
        "clang++ -Winvalid-pch -Xclang -include -Xclang cmake_pch.hxx main.cpp");
    EXPECT_STRIP("cl.exe /Yufoo.h /FIfoo.h /Fpfoo.h_v143.pch /c /Fomain.cpp.o main.cpp",
                 "cl.exe -include foo.h main.cpp");

    /// TODO: Test more commands from other build system.
}

TEST(Command, Reuse) {
    using namespace std::literals;

    CompilationDatabase database;
    database.update_command("fake", "test.cpp", "clang++ -std=c++23 test.cpp"sv);
    database.update_command("fake", "test2.cpp", "clang++ -std=c++23 test2.cpp"sv);

    auto command1 = database.get_command("test.cpp").arguments;
    auto command2 = database.get_command("test2.cpp").arguments;
    ASSERT_EQ(command1.size(), 3);
    ASSERT_EQ(command2.size(), 3);

    EXPECT_EQ(command1[0], "clang++"sv);
    EXPECT_EQ(command1[1], "-std=c++23"sv);
    EXPECT_EQ(command1[2], "test.cpp"sv);

    EXPECT_EQ(command1[0], command2[0]);
    EXPECT_EQ(command1[1], command2[1]);
    EXPECT_EQ(command2[2], "test2.cpp"sv);
}

TEST(Command, Module) {}

TEST(Command, QueryDriver) {
#if __linux__
    using namespace std::literals;

    CompilationDatabase database;
    auto info = database.query_driver("g++");
    ASSERT_TRUE(info);

    EXPECT_EQ(info->target, "x86_64-linux-gnu");
    EXPECT_EQ(info->system_includes.size(), 6);

    if(_GLIBCXX_RELEASE == 13) {
        EXPECT_EQ(info->system_includes[0], "/usr/include/c++/13"sv);
        EXPECT_EQ(info->system_includes[1], "/usr/include/x86_64-linux-gnu/c++/13"sv);
        EXPECT_EQ(info->system_includes[2], "/usr/include/c++/13/backward"sv);
    } else if(_GLIBCXX_RELEASE == 14) {
        EXPECT_EQ(info->system_includes[0], "/usr/include/c++/14"sv);
        EXPECT_EQ(info->system_includes[1], "/usr/include/x86_64-linux-gnu/c++/14"sv);
        EXPECT_EQ(info->system_includes[2], "/usr/include/c++/14/backward"sv);
    }

    EXPECT_EQ(info->system_includes[3], "/usr/local/include"sv);
    EXPECT_EQ(info->system_includes[4], "/usr/include/x86_64-linux-gnu"sv);
    EXPECT_EQ(info->system_includes[5], "/usr/include"sv);

    info = database.query_driver("clang++");
    ASSERT_TRUE(info);

    EXPECT_EQ(info->target, "x86_64-unknown-linux-gnu");
    EXPECT_EQ(info->system_includes.size(), 6);

    if(_GLIBCXX_RELEASE == 13) {
        EXPECT_EQ(info->system_includes[0], "/usr/include/c++/13"sv);
        EXPECT_EQ(info->system_includes[1], "/usr/include/x86_64-linux-gnu/c++/13"sv);
        EXPECT_EQ(info->system_includes[2], "/usr/include/c++/13/backward"sv);
    } else if(_GLIBCXX_RELEASE == 14) {
        EXPECT_EQ(info->system_includes[0], "/usr/include/c++/14"sv);
        EXPECT_EQ(info->system_includes[1], "/usr/include/x86_64-linux-gnu/c++/14"sv);
        EXPECT_EQ(info->system_includes[2], "/usr/include/c++/14/backward"sv);
    }

    EXPECT_EQ(info->system_includes[3], "/usr/local/include"sv);
    EXPECT_EQ(info->system_includes[4], "/usr/include/x86_64-linux-gnu"sv);
    EXPECT_EQ(info->system_includes[5], "/usr/include"sv);
#endif
}

TEST(Command, ResourceDir) {
    /// CompilationDatabase database;
    /// database.update_command("test.cpp", "clang++ -std=c++23 test.cpp");
    /// auto command = database.get_command("test.cpp", false, true);
    /// ASSERT_EQ(command.size(), 4);
    ///
    /// using namespace std::literals;
    /// EXPECT_EQ(command[0], "clang++"sv);
    /// EXPECT_EQ(command[1], "-std=c++23"sv);
    /// EXPECT_EQ(command[2], "test.cpp"sv);
    /// EXPECT_EQ(command[3], std::format("-resource-dir={}", fs::resource_dir));
}

}  // namespace

}  // namespace clice::testing
