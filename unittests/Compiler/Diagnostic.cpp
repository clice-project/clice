#include "Basic/SourceConverter.h"
#include "Support/JSON.h"
#include "Test/CTest.h"
#include "Compiler/Diagnostic.h"

#include <clang/Basic/DiagnosticLex.h>
#include <clang/Basic/DiagnosticSema.h>
#include <llvm/Support/raw_ostream.h>

namespace clice::testing {

namespace {

using namespace clice;

const DiagOption DefaultOption = {};

struct DiagTester : public Tester {
    using Tester::Tester;

    std::vector<Diagnostic> result = {};

    const clang::tidy::ClangTidyContext* tidy = nullptr;

    Tester& run(llvm::StringRef extraFlag = "", const DiagOption& option = DefaultOption) {
        params.command = std::format("clang++ {} {} {}", "-std=c++20", extraFlag, params.srcPath);

        auto info = compile(params, result, option, tidy);
        if(!info) {
            llvm::errs() << "Failed to build AST\n";
            std::terminate();
        }

        this->info.emplace(std::move(*info));
        return *this;
    }
};

void debug(const Diagnostic& diag) {
    llvm::outs() << std::format("severity:{} name: {}, category: {}, id:{}, message: {}, tags:{}",
                                refl::enum_name(diag.severity),
                                json::serialize(diag.name),
                                json::serialize(diag.category),
                                json::serialize(diag.ID),
                                json::serialize(diag.message),
                                json::serialize(diag.tag))
                 << '\n';
}

void debug(const std::vector<Diagnostic>& diags) {
    for(const auto& diag: diags) {
        debug(diag);
    }
}

struct Diagnostics : public ::testing::Test {
    std::optional<DiagTester> tester;

    SourceConverter cvtr = SourceConverter(proto::PositionEncodingKind::UTF8);

    void run(llvm::StringRef source,
             llvm::StringRef extra,
             const DiagOption& option = DefaultOption) {
        tester.emplace("main.cpp", source);
        tester->run(extra, option);
    }

    void runWithHeader(llvm::StringRef source,
                       llvm::StringRef header,
                       llvm::StringRef extra,
                       const DiagOption& option = DefaultOption) {
        tester.emplace("main.cpp", source);
        auto headerPath = path::join(".", "header.h");
        tester->addFile(headerPath, header);
        tester->run(extra, option);
    }

    void EXPECT_DIAG_COUNT(size_t count) {
        auto& res = tester->info->diagnostics();

        EXPECT_EQ(count, res.size());
    }

    void EXPECT_DIAG_RANGE(llvm::StringRef begin,
                           llvm::StringRef end,
                           size_t index,
                           unsigned ID,
                           size_t tagNum) {
        auto& res = tester->info->diagnostics();
        const Diagnostic& diag = res[index];

        // debug(diag);

        auto& SM = tester->info->srcMgr();
        {
            auto lhs = tester->pos(begin);
            auto rhs = cvtr.toPosition(diag.range.getBegin(), SM);
            EXPECT_EQ(lhs, rhs);
        }

        {
            auto lhs = tester->pos(end);
            auto rhs = cvtr.toPosition(diag.range.getEnd(), SM);
            EXPECT_EQ(lhs, rhs);
        }

        EXPECT_EQ(ID, diag.ID);
        EXPECT_EQ(tagNum, diag.tag.size());
    }

    void EXPECT_DIAG_AT(size_t index, unsigned ID, size_t tagNum) {
        auto& res = tester->info->diagnostics();
        const Diagnostic& diag = res[index];

        EXPECT_EQ(ID, diag.ID);
        EXPECT_EQ(tagNum, diag.tag.size());
    }

    auto& result() {
        return tester->info->diagnostics();
    }
};

TEST_F(Diagnostics, Basic) {
    const char* code = R"cpp(
int f() {
    int $(b)xxxxx$(e) = 1;
    return 0;
}

)cpp";

    /// test.cpp:2:9: warning: unused variable 'x' [-Wunused-variable]
    ///     2 |     int xxxxx = 1;
    ///       |         ^
    /// 1 warning generated.
    run(code, "-Wall");

    EXPECT_DIAG_COUNT(1);

    /// FIXME:
    /// The right side of diagnostic should be 'e';
    EXPECT_DIAG_RANGE("b", "b", 0, clang::diag::warn_unused_variable, 1);
}

TEST_F(Diagnostics, WithHeader) {
    const char* header = R"cpp(
int g() {
    int x = 1;
    return 0;
}
)cpp";

    const char* source = R"cpp(
#include "header.h"

int f() {
    int x = 1;
    return 0;
}
)cpp";

    {
        // 'header.h' file not found
        run(source, "-Wall");
        EXPECT_DIAG_COUNT(1);
        EXPECT_DIAG_AT(0, clang::diag::err_pp_file_not_found, 0);
    }

    runWithHeader(source, header, "-Wall");
    EXPECT_DIAG_COUNT(2);
    EXPECT_DIAG_AT(0, clang::diag::warn_unused_variable, 1);
    EXPECT_DIAG_AT(1, clang::diag::warn_unused_variable, 1);
}

}  // namespace

}  // namespace clice::testing

