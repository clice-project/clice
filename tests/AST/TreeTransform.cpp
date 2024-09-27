#include <gtest/gtest.h>
#include <AST/Resolver.h>
#include <clang-extra/TreeTransform.h>

namespace {

using namespace clice;

std::vector<const char*> compileArgs = {
    "clang++",
    "-std=c++20",
    "main.cpp",
    "-resource-dir",
    "/home/ykiko/C++/clice2/build/lib/clang/20",
};

class TemplateResolver : public clang::TreeTransform<TemplateResolver> {
    using Base = clang::TreeTransform<TemplateResolver>;

    struct Frame {
        clang::NamedDecl* decl;
        std::vector<clang::TemplateArgument> arguments;
    };

public:
    TemplateResolver(clang::Sema& sema, clang::ASTContext& context) : TreeTransform(sema), context(context) {}

private:
    std::vector<Frame> frames;
    clang::ASTContext& context;
};

class Replacer : public clang::TreeTransform<Replacer> {
    clang::ASTContext& context;

    using Base = clang::TreeTransform<Replacer>;

public:
    Replacer(clang::Sema& sema, clang::ASTContext& context) : TreeTransform{sema}, context(context) {}

public:
    bool TransformTemplateArguments(const clang::TemplateArgumentLoc* Inputs,
                                    unsigned NumInputs,
                                    clang::TemplateArgumentListInfo& Outputs,
                                    bool Uneval = false) {
        return TransformTemplateArguments(Inputs, Inputs + NumInputs, Outputs, Uneval);
    }

    template <typename InputIterator>
    bool TransformTemplateArguments(InputIterator First,
                                    InputIterator Last,
                                    clang::TemplateArgumentListInfo& Outputs,
                                    bool Uneval = false) {
        llvm::outs() << "template\n";
        for(auto it = First; it != Last; ++it) {
            it->getArgument().dump();
        }
        return Base::TransformTemplateArguments(First, Last, Outputs, Uneval);
    }

    clang::NestedNameSpecifierLoc TransformNestedNameSpecifierLoc(clang::NestedNameSpecifierLoc NNS,
                                                                  clang::QualType ObjectType = clang::QualType(),
                                                                  clang::NamedDecl* FirstQualifierInScope = nullptr) {
        llvm::outs() << "nested\n";
        return Base::TransformNestedNameSpecifierLoc(NNS, ObjectType, FirstQualifierInScope);
    }

    clang::QualType TransformDependentNameType(clang::TypeLocBuilder& TLB,
                                               clang::DependentNameTypeLoc TL,
                                               bool DeducedTSTContext = false) {

        DependentNameResolver resolver{getSema(), context};
        auto NNS = TransformNestedNameSpecifierLoc(TL.getQualifierLoc());
        auto DNT = context.getDependentNameType(TL.getTypePtr()->getKeyword(),
                                                NNS.getNestedNameSpecifier(),
                                                TL.getTypePtr()->getIdentifier());
        auto result = resolver.resolve(DNT);
        result->dump();
        llvm::outs() << "--------------------------------------------\n";

        switch(clang::TypeLoc(result, nullptr).getTypeLocClass()) {
#define ABSTRACT_TYPELOC(CLASS, PARENT)
#define TYPELOC(CLASS, PARENT)                                                                                         \
    case clang::TypeLoc::CLASS: TLB.push<clang::CLASS##TypeLoc>(result); break;
#include "clang/AST/TypeLocNodes.def"
        }

        return result;
    }
};

struct Visitor : public clang::RecursiveASTVisitor<Visitor> {
    clang::QualType result;
    std::unique_ptr<ParsedAST> parsedAST;

    Visitor(const char* code) : parsedAST(ParsedAST::build("main.cpp", code, compileArgs)) {}

    bool VisitTypeAliasDecl(clang::TypeAliasDecl* decl) {
        if(decl->getName() == "result") {
            Replacer replacer(parsedAST->sema, parsedAST->context);
            // decl->getTypeSourceInfo()->getTypeLoc().getNextTypeLoc().getType().dump();
            auto result = replacer.TransformType(decl->getTypeSourceInfo());
            // result->getType().dump();
        }
        return true;
    }

    clang::QualType test() {
        clang::TypeLocBuilder builder;
        auto decl = parsedAST->context.getTranslationUnitDecl();
        TraverseDecl(decl);
        return result;
    }
};

TEST(Transform, test) {

    const char* code = R"(
template <typename ...Ts>
struct type_list {};

template <typename T>
struct A {
    using type = type_list<T>;
};

template <typename X>
struct test {
    using result = typename A<typename A<X>::type>::type&;
};

)";

    Visitor visitor(code);
    auto result = visitor.test();
}

}  // namespace

