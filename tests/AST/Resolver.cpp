#include <Test/Test.h>
#include <Compiler/Resolver.h>
#include <Compiler/Compiler.h>

namespace {

using namespace clice;

std::vector<const char*> compileArgs = {
    "clang++",
    "-std=c++20",
    "main.cpp",
    "-resource-dir",
    "/home/ykiko/C++/clice2/build/lib/clang/20",
};

struct Visitor : public clang::RecursiveASTVisitor<Visitor> {
    clang::QualType result;
    clang::QualType expect;
    clice::Compiler compiler;

    Visitor(llvm::StringRef content) : compiler("main.cpp", content, compileArgs) {
        compiler.buildAST();
    }

    bool VisitTypeAliasDecl(clang::TypeAliasDecl* decl) {
        if(decl->getName() == "result") {
            TemplateResolver resolver(compiler.sema());
            result = resolver.resolve(decl->getUnderlyingType());
        }

        if(decl->getName() == "expect") {
            expect = decl->getUnderlyingType();
        }

        return true;
    }

    void test() {
        auto decl = compiler.context().getTranslationUnitDecl();
        TraverseDecl(decl);
        EXPECT_EQ(result.getCanonicalType(), expect.getCanonicalType());
    }
};

void match(clang::QualType type, std::string name, std::initializer_list<std::string> args) {
    auto TST = type->getAs<clang::TemplateSpecializationType>();
    ASSERT_TRUE(TST);
    ASSERT_EQ(TST->getTemplateName().getAsTemplateDecl()->getName(), name);

    auto template_args = TST->template_arguments();
    ASSERT_EQ(template_args.size(), args.size());

    auto iter = args.begin();
    for(auto arg: template_args) {
        auto T = llvm::dyn_cast<clang::TemplateTypeParmType>(arg.getAsType());
        ASSERT_TRUE(T);
        ASSERT_EQ(T->getDecl()->getName(), *iter);
        ++iter;
    }
}

TEST(clice, TemplateResolver) {
    foreachFile("TemplateResolver", [&](std::string file, llvm::StringRef content) {
        Visitor visitor(content);
        visitor.test();
        llvm::outs() << fmt::format(fg(fmt::color::yellow_green), "[TemplateResolver: {}]\n", file);
    });
}

}  // namespace

