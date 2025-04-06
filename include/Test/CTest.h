#pragma once

#include "Test.h"
#include "Annotation.h"
#include "Server/Protocol.h"
#include "Compiler/Compilation.h"

namespace clice::testing {

struct Tester {
    CompilationParams params;
    std::optional<ASTInfo> AST;

    /// Annoated locations.
    llvm::StringMap<std::uint32_t> offsets;
    std::vector<std::string> sources;

public:
    Tester() = default;

    Tester(llvm::StringRef file, llvm::StringRef content) {
        params.srcPath = file;
        params.content = annoate(content);
    }

    void addMain(llvm::StringRef file, llvm::StringRef content) {
        params.srcPath = file;
        params.content = annoate(content);
    }

    void addFile(llvm::StringRef name, llvm::StringRef content) {
        params.addRemappedFile(name, annoate(content));
    }

    llvm::StringRef annoate(llvm::StringRef content) {
        auto& source = sources.emplace_back();
        source.reserve(content.size());

        uint32_t line = 0;
        uint32_t column = 0;
        uint32_t offset = 0;

        for(uint32_t i = 0; i < content.size();) {
            auto c = content[i];

            if(c == '@') {
                i += 1;
                auto key = content.substr(i).take_until([](char c) { return c == ' '; });
                offsets.try_emplace(key, offset);
                continue;
            }

            if(c == '$') {
                assert(i + 1 < content.size() && content[i + 1] == '(' && "expect $(name)");
                i += 2;
                auto key = content.substr(i).take_until([](char c) { return c == ')'; });
                i += key.size() + 1;
                offsets.try_emplace(key, offset);
                continue;
            }

            if(c == '\n') {
                line += 1;
                column = 0;
            } else {
                column += 1;
            }

            i += 1;
            offset += 1;

            source.push_back(c);
        }

        return source;
    }

    Tester& compile(llvm::StringRef standard = "-std=c++20") {
        params.command = std::format("clang++ {} {} -fms-extensions", standard, params.srcPath);
        auto info = clice::compile(params);
        ASSERT_TRUE(info);
        this->AST.emplace(std::move(*info));
        return *this;
    }

    std::uint32_t offset(llvm::StringRef key) const {
        return offsets.lookup(key);
    }
};

struct TestFixture : ::testing::Test, Tester {};

}  // namespace clice::testing

