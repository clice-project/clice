#include "Test/Test.h"
#include "Compiler/Command.h"

namespace clice::testing {

namespace {

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

TEST(Command, Merge) {}

TEST(Command, Filter) {}

TEST(Command, QueryDriver) {}

TEST(Command, Module) {}

}  // namespace

}  // namespace clice::testing
