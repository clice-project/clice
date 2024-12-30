#pragma once

#include <gtest/gtest.h>
#include <Basic/Location.h>
#include <Compiler/Compiler.h>
#include <Support/Support.h>

namespace clice {

namespace test {

llvm::StringRef source_dir();

llvm::StringRef resource_dir();

}  // namespace test

class Annotation {
public:
    Annotation(llvm::StringRef source) : m_source() {
        m_source.reserve(source.size());

        uint32_t line = 0;
        uint32_t column = 0;

        for(uint32_t i = 0; i < source.size();) {
            auto c = source[i];

            if(c == '@') {
                i += 1;
                auto key = source.substr(i).take_until([](char c) { return c == ' '; });
                assert(!locations.contains(key) && "duplicate key");
                locations.try_emplace(key, line, column);
                continue;
            }

            if(c == '$') {
                assert(i + 1 < source.size() && source[i + 1] == '(' && "expect $(name)");
                i += 2;
                auto key = source.substr(i).take_until([](char c) { return c == ')'; });
                i += key.size() + 1;
                assert(!locations.contains(key) && "duplicate key");
                locations.try_emplace(key, line, column);
                continue;
            }

            if(c == '\n') {
                line += 1;
                column = 0;
            } else {
                column += 1;
            }

            i += 1;
            m_source.push_back(c);
        }
    }

    llvm::StringRef source() const {
        return m_source;
    }

    proto::Position position(llvm::StringRef key) const {
        return locations.lookup(key);
    }

private:
    std::string m_source;
    llvm::StringMap<proto::Position> locations;
};

template <typename Callback>
inline void foreachFile(std::string name, const Callback& callback) {
    llvm::SmallString<128> path;
    path += test::source_dir();
    path::append(path, name);
    std::error_code error;
    fs::directory_iterator iter(path, error);
    fs::directory_iterator end;
    while(!error && iter != end) {
        auto file = iter->path();
        auto buffer = llvm::MemoryBuffer::getFile(file);
        if(!buffer) {
            llvm::outs() << "failed to open file: " << buffer.getError().message() << file << "\n";
            // TODO:
        }
        auto content = buffer.get()->getBuffer();
        callback(file, content);
        iter.increment(error);
    }
}

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

class Tester {
public:
    CompilationParams params;
    std::unique_ptr<llvm::vfs::InMemoryFileSystem> vfs;
    ASTInfo info;

    /// Annoated locations.
    std::vector<std::string> sources;
    llvm::StringMap<proto::Position> locations;
    llvm::StringMap<std::uint32_t> offsets;

public:
    Tester(llvm::StringRef file, llvm::StringRef content) {
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
        params.vfs = std::move(vfs);
        params.command = std::format("clang++ {} {}", standard, params.srcPath);

        auto info = compile(params);
        if(!info) {
            llvm::errs() << "Failed to build AST\n";
            std::terminate();
        }

        this->info = std::move(*info);
        return *this;
    }

    Tester& fail(const auto& lhs, const auto& rhs, std::source_location loc) {
        auto msg =
            std::format("left : {}\nright: {}\n", json::serialize(lhs), json::serialize(rhs));
        ::testing::internal::AssertHelper(::testing ::TestPartResult ::kFatalFailure,
                                          loc.file_name(),
                                          loc.line(),
                                          msg.c_str()) = ::testing ::Message();
        return *this;
    }

    Tester& equal(const auto& lhs,
                  const auto& rhs,
                  std::source_location loc = std::source_location::current()) {
        if(!refl::equal(lhs, rhs)) {
            return fail(lhs, rhs, loc);
        }
        return *this;
    }

    Tester& expect(llvm::StringRef name,
                   clang::SourceLocation loc,
                   std::source_location current = std::source_location::current()) {
        auto pos = locations.lookup(name);
        auto presumed = info.srcMgr().getPresumedLoc(loc);
        /// FIXME:
        equal(pos, proto::Position{presumed.getLine() - 1, presumed.getColumn() - 1}, current);
        return *this;
    }
};

}  // namespace clice

