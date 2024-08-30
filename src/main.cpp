#include <clang/AST/Decl.h>
#include <cassert>
#include <AST/Diagnostic.h>
#include <AST/ParsedAST.h>

const char* source = R"(
int main(){
    return 0;
}
)";

int main() {
    auto args = std::vector<const char*>{"clang++", "main.cpp"};
    auto AST = clice::ParsedAST::build("main.cpp", source, args, nullptr);
    AST->TranslationUnitDecl()->dump();
}
