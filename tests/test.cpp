#include <AST/Resolver.h>

using namespace clice;

const char* code = R"(
#include <vector>

template <typename T>
struct X {};

template <typename T, template <typename ...> typename List>
struct X<List<T>> {
    using type = std::vector<T>;
};

template <typename T, typename U>
struct test {
    using result = typename X<X<U>>::type;
};
)";

struct ASTVisitor : public clang::RecursiveASTVisitor<ASTVisitor> {
    ParsedAST& ast;

    bool VisitTypeAliasDecl(clang::TypeAliasDecl* decl) {
        if(decl->getName() == "result") {
            auto type = decl->getUnderlyingType();
            clang::Sema::CodeSynthesisContext context;
            context.Kind = clang::Sema::CodeSynthesisContext::TemplateInstantiation;
            context.Entity = decl;
            context.TemplateArgs = nullptr;
            ast.sema.pushCodeSynthesisContext(context);
            auto resolver = DependentNameResolver(ast.sema, ast.context);
            auto resolved = resolver.resolve(type);
            resolved->dump();
        }
        return true;
    }
};

int main() {
    std::vector<const char*> args{
        "clang++",
        "-std=c++20",
        "main.cpp",
        "-resource-dir=/home/ykiko/C++/clice2/build/lib/clang/20",
    };
    auto ast = ParsedAST::build("main.cpp", code, args, nullptr);
    auto decl = ast->context.getTranslationUnitDecl();

    ASTVisitor visitor{.ast = *ast};

    visitor.TraverseDecl(decl);

    return 0;
}
