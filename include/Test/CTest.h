#pragma once

#include "gtest/gtest.h"
#include "Basic/Location.h"
#include "Compiler/Compilation.h"
#include "Support/Support.h"

namespace clice::testing {

#undef EXPECT_EQ
#undef EXPECT_NE

inline void EXPECT_FAILURE(std::string msg,
                           std::source_location current = std::source_location::current()) {
    ::testing::internal::AssertHelper(::testing ::TestPartResult ::kNonFatalFailure,
                                      current.file_name(),
                                      current.line(),
                                      msg.c_str()) = ::testing ::Message();
}

template <typename LHS, typename RHS>
inline void EXPECT_EQ(const LHS& lhs,
                      const RHS& rhs,
                      std::source_location current = std::source_location::current()) {
    if(!refl::equal(lhs, rhs)) {
        std::string expect;
        if constexpr(requires { sizeof(json::Serde<LHS>); }) {
            llvm::raw_string_ostream(expect) << json::serialize(lhs);
        } else {
            expect = "cannot dump value";
        }

        std::string actual;
        if constexpr(requires { sizeof(json::Serde<RHS>); }) {
            llvm::raw_string_ostream(actual) << json::serialize(rhs);
        } else {
            actual = "cannot dump value";
        }

        EXPECT_FAILURE(std::format("expect: {}, actual: {}\n", expect, actual), current);
    }
}

template <typename LHS, typename RHS>
inline void EXPECT_NE(const LHS& lhs,
                      const RHS& rhs,
                      std::source_location current = std::source_location::current()) {
    if(refl::equal(lhs, rhs)) {
        std::string expect;
        if constexpr(requires { sizeof(json::Serde<LHS>); }) {
            llvm::raw_string_ostream(expect) << json::serialize(lhs);
        } else {
            expect = "cannot dump value";
        }

        std::string actual;
        if constexpr(requires { sizeof(json::Serde<RHS>); }) {
            llvm::raw_string_ostream(actual) << json::serialize(rhs);
        } else {
            actual = "cannot dump value";
        }

        EXPECT_FAILURE(std::format("expect: {}, actual: {}\n", expect, actual), current);
    }
}

struct Tester {
    CompilationParams params;
    std::optional<ASTInfo> info;

    /// Annoated locations.
    llvm::StringMap<std::uint32_t> offsets;
    llvm::StringMap<proto::Position> locations;
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
        params.remappedFiles.emplace_back(name, content);
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

        return source;
    }

    Tester& run(const char* standard = "-std=c++20") {
        params.command = std::format("clang++ {} {}", standard, params.srcPath);

        auto info = compile(params);
        if(!info) {
            llvm::errs() << "Failed to build AST\n";
            std::terminate();
        }

        this->info.emplace(std::move(*info));
        return *this;
    }

    proto::Position pos(llvm::StringRef key) const {
        return locations.lookup(key);
    }
};

}  // namespace clice::testing

