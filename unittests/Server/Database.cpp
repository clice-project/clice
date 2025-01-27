#include "Test/Test.h"

#include "../../src/Server/Database.cpp"

namespace clice::testing {

namespace {

TEST(CompilationDatabase, Command) {

    /// Create a temorary file for test
    llvm::SmallString<128> path;
    auto err = llvm::sys::fs::createTemporaryFile("clice-unittest-cbd",
                                                  "json",
                                                  path,
                                                  llvm::sys::fs::OF_Text);
    EXPECT_FALSE(err);

    // path: "/tmp/clice-unittest-cbd-6c6040.json"
    auto dir = path::parent_path(path);
    auto filename = path::filename(path);

    auto example = std::format(R"(
[
  {{ 
    "directory": "{0}",
    "arguments": ["/usr/bin/clang++", "-Irelative", "-DSOMEDEF=\"With spaces, quotes and \\-es.\"", "-c", "-o", "file.o", "file.cc"],
    "file": "{1}" 
  }},

  {{ 
    "directory": "{0}",
    "command": "/usr/bin/clang++ -Irelative -DSOMEDEF=\"With spaces, quotes and \\-es.\" -c -o file.o file.cc",
    "file": "{1}" 
  }},

  {{ 
    "command": "/usr/bin/clang++ -Irelative -DSOMEDEF=\"With spaces, quotes and \\-es.\" -c -o file.o file.cc",
    "file": "{2}" 
  }}

]
)",
                               dir,
                               filename,
                               path);

    auto object = llvm::json::parse(example);
    EXPECT_TRUE(bool(object));

    for(auto& value: *object->getAsArray()) {
        auto object = value.getAsObject();
        EXPECT_TRUE(object);

        auto res = tryParseCompileCommand(object);
        EXPECT_TRUE(res.has_value());

        auto [file, command] = std::move(res).value();

        EXPECT_EQ(file, std::format("{}", path.str()));

        llvm::StringRef expectCmd =
            "/usr/bin/clang++ -Irelative -DSOMEDEF=\"With spaces, quotes and \\-es.\" -c -o file.o file.cc";
        EXPECT_EQ(expectCmd, llvm::StringRef(command).rtrim());
    }
}

TEST(CompilationDatabase, Module) {}

}  // namespace

}  // namespace clice::testing
