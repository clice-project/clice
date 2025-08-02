#pragma once

#include "Test.h"
#include "Annotation.h"
#include "Protocol/Protocol.h"
#include "Compiler/Command.h"
#include "Compiler/Compilation.h"

namespace clice::testing {

struct Tester {
    CompilationParams params;
    CompilationDatabase database;
    std::optional<CompilationUnit> unit;
    std::string src_path;

    /// All sources file in the compilation.
    AnnotatedSources sources2;

public:
    Tester() = default;

    Tester(llvm::StringRef file, llvm::StringRef content) {
        src_path = file;
        sources2.add_source(file, content);
    }

    void addMain(llvm::StringRef file, llvm::StringRef content) {
        src_path = file;
        sources2.add_source(file, content);
    }

    void addFile(llvm::StringRef name, llvm::StringRef content) {
        sources2.add_source(name, content);
    }

    Tester& compile(llvm::StringRef standard = "-std=c++20") {
        auto command = std::format("clang++ {} {} -fms-extensions", standard, src_path);

        database.update_command("fake", src_path, command);
        params.arguments = database.get_command(src_path).arguments;

        for(auto& [file, source]: sources2.all_files) {
            params.add_remapped_file(file, source.content);
        }

        auto info = clice::compile(params);
        ASSERT_TRUE(info);
        this->unit.emplace(std::move(*info));
        return *this;
    }

    std::uint32_t offset(llvm::StringRef key) const {
        return sources2.all_files.lookup(src_path).offsets.lookup(key);
    }
};

struct TestFixture : ::testing::Test, Tester {};

}  // namespace clice::testing

