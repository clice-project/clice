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
    AnnotatedSources sources;

    void add_main(llvm::StringRef file, llvm::StringRef content) {
        src_path = file;
        sources.add_source(file, content);
    }

    void add_file(llvm::StringRef name, llvm::StringRef content) {
        sources.add_source(name, content);
    }

    Tester& compile(llvm::StringRef standard = "-std=c++20") {
        auto command = std::format("clang++ {} {} -fms-extensions", standard, src_path);

        database.update_command("fake", src_path, command);
        params.arguments = database.get_command(src_path).arguments;

        for(auto& [file, source]: sources.all_files) {
            params.add_remapped_file(file, source.content);
        }

        auto info = clice::compile(params);
        ASSERT_TRUE(info);
        this->unit.emplace(std::move(*info));
        return *this;
    }

    Tester& compile_with_pch(llvm::StringRef standard = "-std=c++20") {
        auto command = std::format("clang++ {} {} -fms-extensions", standard, src_path);

        database.update_command("fake", src_path, command);
        params.arguments = database.get_command(src_path, true).arguments;

        auto path = fs::createTemporaryFile("clice", "pch");
        if(!path) {
            llvm::outs() << path.error().message() << "\n";
        }

        /// Build PCH
        params.output_file = *path;

        for(auto& [file, source]: sources.all_files) {
            if(file == src_path) {
                auto bound = computePreambleBound(source.content);
                params.add_remapped_file(file, source.content.substr(0, bound));
            } else {
                params.add_remapped_file(file, source.content);
            }
        }

        PCHInfo info;
        {
            auto unit = clice::compile(params, info);
            if(!unit) {
                llvm::outs() << unit.error() << "\n";
            }
        }

        /// Build AST
        params.output_file.clear();
        params.pch = {info.path, info.preamble.size()};
        for(auto& [file, source]: sources.all_files) {
            params.add_remapped_file(file, source.content);
        }

        auto unit = clice::compile(params);
        ASSERT_TRUE(unit);
        this->unit.emplace(std::move(*unit));
        return *this;
    }

    std::uint32_t operator[] (llvm::StringRef file, llvm::StringRef pos) {
        return sources.all_files.lookup(file).offsets.lookup(pos);
    }
};

struct TestFixture : ::testing::Test, Tester {};

}  // namespace clice::testing

