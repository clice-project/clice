#pragma once

#include "Test.h"
#include "Server/Protocol.h"
#include "Compiler/Compilation.h"

namespace clice::testing {

struct Tester {
    CompilationParams params;
    std::optional<ASTInfo> info;

    /// Annoated locations.
    llvm::StringMap<std::uint32_t> offsets;
    llvm::StringMap<proto::Position> locations;
    std::vector<std::string> sources;

    proto::Position eof;

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
        params.remappedFiles.emplace_back(name, content);
    }

    proto::Position endOfFile() const {
        return eof;
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
                assert(!locations.contains(key) && "duplicate key");
                locations.try_emplace(key, line, column);
                offsets.try_emplace(key, offset);
                continue;
            }

            if(c == '$') {
                assert(i + 1 < content.size() && content[i + 1] == '(' && "expect $(name)");
                i += 2;
                auto key = content.substr(i).take_until([](char c) { return c == ')'; });
                i += key.size() + 1;
                assert(!locations.contains(key) && "duplicate key");
                locations.try_emplace(key, line, column);
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

        eof.line = line;
        eof.character = column;
        return source;
    }

    Tester& run(const char* standard = "-std=c++20") {
        params.command = std::format("clang++ {} {} -fms-extensions", standard, params.srcPath);

        auto info = compile(params);
        if(!info) {
            llvm::errs() << "Failed to build AST\n";
            std::abort();
        }

        this->info.emplace(std::move(*info));
        return *this;
    }

    proto::Position pos(llvm::StringRef key) const {
        return locations.lookup(key);
    }

    std::uint32_t offset(llvm::StringRef key) const {
        return offsets.lookup(key);
    }
};

struct Test : ::testing::Test, Tester {};

}  // namespace clice::testing

