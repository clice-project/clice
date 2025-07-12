#include "Test/Test.h"
#include "Compiler/Command.h"

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

std::string strip(llvm::StringRef arg, llvm::StringRef argv) {
    llvm::SmallVector<llvm::StringRef> parts;
    llvm::SplitString(argv, parts);
    std::vector<const char*> args;
    std::deque<std::string> args_storage;
    for(auto part: parts) {
        args_storage.emplace_back(part);
        args.emplace_back(args_storage.back().c_str());
    }
    ArgStripper S;
    S.strip(arg);
    S.process(args);
    return printArgv(args);
}

TEST(Command, Strip) {
    // May use alternate prefixes.
    EXPECT_EQ(strip("-pedantic", "clang -pedantic foo.cc"), "clang foo.cc");
    EXPECT_EQ(strip("-pedantic", "clang --pedantic foo.cc"), "clang foo.cc");
    EXPECT_EQ(strip("--pedantic", "clang -pedantic foo.cc"), "clang foo.cc");
    EXPECT_EQ(strip("--pedantic", "clang --pedantic foo.cc"), "clang foo.cc");
    // May use alternate names.
    EXPECT_EQ(strip("-x", "clang -x c++ foo.cc"), "clang foo.cc");
    EXPECT_EQ(strip("-x", "clang --language=c++ foo.cc"), "clang foo.cc");
    EXPECT_EQ(strip("--language=", "clang -x c++ foo.cc"), "clang foo.cc");
    EXPECT_EQ(strip("--language=", "clang --language=c++ foo.cc"), "clang foo.cc");

    /// UnknownFlag.
    EXPECT_EQ(strip("-xyzzy", "clang -xyzzy foo.cc"), "clang foo.cc");
    EXPECT_EQ(strip("-xyz*", "clang -xyzzy foo.cc"), "clang foo.cc");
    EXPECT_EQ(strip("-xyzzy", "clang -Xclang -xyzzy foo.cc"), "clang foo.cc");

    // Flags may be -Xclang escaped.
    EXPECT_EQ(strip("-ast-dump", "clang -Xclang -ast-dump foo.cc"), "clang foo.cc");
    // args may be -Xclang escaped.
    EXPECT_EQ(strip("-add-plugin", "clang -Xclang -add-plugin -Xclang z foo.cc"), "clang foo.cc");

    // /I is a synonym for -I in clang-cl mode only.
    // Not stripped by default.
    EXPECT_EQ(strip("-I", "clang -I /usr/inc /Interesting/file.cc"), "clang /Interesting/file.cc");
    // Stripped when invoked as clang-cl.
    EXPECT_EQ(strip("-I", "clang-cl -I /usr/inc /Interesting/file.cc"), "clang-cl");
    // Stripped when invoked as CL.EXE
    EXPECT_EQ(strip("-I", "CL.EXE -I /usr/inc /Interesting/file.cc"), "CL.EXE");
    // Stripped when passed --driver-mode=cl.
    EXPECT_EQ(strip("-I", "cc -I /usr/inc /Interesting/file.cc --driver-mode=cl"),
              "cc --driver-mode=cl");

    // Flag
    EXPECT_EQ(strip("-Qn", "clang -Qn foo.cc"), "clang foo.cc");
    EXPECT_EQ(strip("-Qn", "clang -QnZ foo.cc"), "clang -QnZ foo.cc");
    // Joined
    EXPECT_EQ(strip("-std=", "clang -std= foo.cc"), "clang foo.cc");
    EXPECT_EQ(strip("-std=", "clang -std=c++11 foo.cc"), "clang foo.cc");
    // Separate
    EXPECT_EQ(strip("-mllvm", "clang -mllvm X foo.cc"), "clang foo.cc");
    EXPECT_EQ(strip("-mllvm", "clang -mllvmX foo.cc"), "clang -mllvmX foo.cc");
    // RemainingArgsJoined
    EXPECT_EQ(strip("/link", "clang-cl /link b c d foo.cc"), "clang-cl");
    EXPECT_EQ(strip("/link", "clang-cl /linka b c d foo.cc"), "clang-cl");
    // CommaJoined
    EXPECT_EQ(strip("-Wl,", "clang -Wl,x,y foo.cc"), "clang foo.cc");
    EXPECT_EQ(strip("-Wl,", "clang -Wl, foo.cc"), "clang foo.cc");
    // MultiArg
    EXPECT_EQ(strip("-segaddr", "clang -segaddr a b foo.cc"), "clang foo.cc");
    EXPECT_EQ(strip("-segaddr", "clang -segaddra b foo.cc"), "clang -segaddra b foo.cc");
    // JoinedOrSeparate
    EXPECT_EQ(strip("-G", "clang -GX foo.cc"), "clang foo.cc");
    EXPECT_EQ(strip("-G", "clang -G X foo.cc"), "clang foo.cc");
    // JoinedAndSeparate
    EXPECT_EQ(strip("-plugin-arg-", "clang -cc1 -plugin-arg-X Y foo.cc"), "clang -cc1 foo.cc");
    EXPECT_EQ(strip("-plugin-arg-", "clang -cc1 -plugin-arg- Y foo.cc"), "clang -cc1 foo.cc");

    // When we hit the end-of-args prematurely, we don't crash.
    // We consume the incomplete args if we've matched the target option.
    EXPECT_EQ(strip("-I", "clang -Xclang"), "clang -Xclang");
    EXPECT_EQ(strip("-I", "clang -Xclang -I"), "clang");
    EXPECT_EQ(strip("-I", "clang -I -Xclang"), "clang");
    EXPECT_EQ(strip("-I", "clang -I"), "clang");

    {
        ArgStripper s;
        s.strip("-o");
        s.strip("-c");
        std::vector<const char*> args = {"clang", "-o", "foo.o", "foo.cc", "-c"};
        s.process(args);
        EXPECT_EQ(args, std::vector{"clang", "foo.cc"});
    }

    {
        // -W is a flag name
        ArgStripper s;
        s.strip("-W");
        std::vector<const char*> args = {"clang", "-Wfoo", "-Wno-bar", "-Werror", "foo.cc"};
        s.process(args);
        EXPECT_EQ(args, std::vector{"clang", "foo.cc"});
    }
    {
        // -Wfoo is not a flag name, matched literally.
        ArgStripper S;
        S.strip("-Wunused");
        std::vector<const char*> args = {"clang", "-Wunused", "-Wno-unused", "foo.cc"};
        S.process(args);
        EXPECT_EQ(args, std::vector{"clang", "-Wno-unused", "foo.cc"});
    }

    {
        // -D is a flag name
        ArgStripper S;
        S.strip("-D");
        std::vector<const char*> args = {"clang", "-Dfoo", "-Dbar=baz", "foo.cc"};
        S.process(args);
        EXPECT_EQ(args, std::vector{"clang", "foo.cc"});
    }
    {
        // -Dbar is not: matched literally
        ArgStripper S;
        S.strip("-Dbar");
        std::vector<const char*> args = {"clang", "-Dfoo", "-Dbar=baz", "foo.cc"};
        S.process(args);
        EXPECT_EQ(args, std::vector{"clang", "-Dfoo", "-Dbar=baz", "foo.cc"});
        S.strip("-Dfoo");
        S.process(args);
        EXPECT_EQ(args, std::vector{"clang", "-Dbar=baz", "foo.cc"});
        S.strip("-Dbar=*");
        S.process(args);
        EXPECT_EQ(args, std::vector{"clang", "foo.cc"});
    }

    {
        ArgStripper S;
        // If -include is stripped first, we see -pch as its arg and foo.pch remains.
        // To get this case right, we must process -include-pch first.
        S.strip("-include");
        S.strip("-include-pch");
        std::vector<const char*> args = {"clang", "-include-pch", "foo.pch", "foo.cc"};
        S.process(args);
        EXPECT_EQ(args, std::vector{"clang", "foo.cc"});
    }
}

TEST(Command, SimpleAddGet) {
    CompilationDatabase database;
    database.add_command("test.cpp", "clang++ -std=c++23 test.cpp");
    auto command = database.get_command("test.cpp");
    ASSERT_EQ(command.size(), 3);

    using namespace std::literals;
    EXPECT_EQ(command[0], "clang++"sv);
    EXPECT_EQ(command[1], "-std=c++23"sv);
    EXPECT_EQ(command[2], "test.cpp"sv);
}

TEST(Command, Reuse) {
    CompilationDatabase database;
    database.add_command("test.cpp", "clang++ -std=c++23 test.cpp");
    database.add_command("test2.cpp", "clang++ -std=c++23 test2.cpp");

    auto command1 = database.get_command("test.cpp");
    auto command2 = database.get_command("test2.cpp");
    ASSERT_EQ(command1.size(), 3);
    ASSERT_EQ(command2.size(), 3);

    using namespace std::literals;
    EXPECT_EQ(command1[0], "clang++"sv);
    EXPECT_EQ(command1[1], "-std=c++23"sv);
    EXPECT_EQ(command1[2], "test.cpp"sv);

    EXPECT_EQ(command1[0], command2[0]);
    EXPECT_EQ(command1[1], command2[1]);
    EXPECT_EQ(command2[2], "test2.cpp"sv);
}

TEST(Command, ResourceDir) {
    CompilationDatabase database;
    database.add_command("test.cpp", "clang++ -std=c++23 test.cpp");
    auto command = database.get_command("test.cpp", false, true);
    ASSERT_EQ(command.size(), 4);

    using namespace std::literals;
    EXPECT_EQ(command[0], "clang++"sv);
    EXPECT_EQ(command[1], "-std=c++23"sv);
    EXPECT_EQ(command[2], "test.cpp"sv);
    EXPECT_EQ(command[3], std::format("-resource-dir={}", fs::resource_dir));
}

TEST(Command, Filter) {
    CompilationDatabase database;
    database.add_command("test.cpp", "clang++ -c -o test.o test.cpp");
    auto command = database.get_command("test.cpp");

    using namespace std::literals;
    EXPECT_EQ(command.size(), 2);
    EXPECT_EQ(command[0], "clang++"sv);
    EXPECT_EQ(command[1], "test.cpp"sv);

    database.add_command("test1.cpp", "clang++ -c -otest1.o test1.cpp");
    command = database.get_command("test1.cpp");
    EXPECT_EQ(command.size(), 2);
    EXPECT_EQ(command[0], "clang++"sv);
    EXPECT_EQ(command[1], "test1.cpp"sv);
}

TEST(Command, Merge) {}

TEST(Command, QueryDriver) {}

TEST(Command, Module) {}

}  // namespace

}  // namespace clice::testing
