#include <clang/AST/Decl.h>
#include <cassert>
#include <AST/Diagnostic.h>
#include <AST/ParsedAST.h>
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"
#include "clang/Sema/TemplateDeduction.h"

// llvm::FoldingSetNodeID id;
// clang::ClassTemplateSpecializationDecl::Profile(id, arguments, context);
// for(auto spec: decl->specializations()) {
//     llvm::FoldingSetNodeID id2;
//     spec->Profile(id2);
//     llvm::outs() << "spec profile: " << (id == id2) << "\n";
//     llvm::outs() << "---------------------------------------------------------------\n";
//     for(std::size_t i = 0; i < arguments.size(); i++) {
//         llvm::FoldingSetNodeID id3;
//         arguments[i].Profile(id3, context);
//         llvm::FoldingSetNodeID id4;
//         spec->getTemplateArgs().asArray()[i].Profile(id4, context);
//         llvm::outs() << "argument profile: " << (id3 == id4) << "\n";
//         arguments[i].dump();
//         spec->getTemplateArgs().asArray()[i].dump();
//         llvm::outs() << "---------------------------------------------------------------\n";
//     }
// }

// if(auto specialization = decl->findSpecialization(arguments, pos)) {
//     specialization->dump();
// }

// llvm::SmallVector<clang::ClassTemplatePartialSpecializationDecl*> partials;
// decl->getPartialSpecializations(partials);
// for(auto partial: partials) {
//     if(auto specialization =
//            decl->findPartialSpecialization(arguments, partial->getTemplateParameters(), pos)) {
//         specialization->dump();
//     }
// }

const char* source = R"(
#include <vector>

template <typename _Tp, typename _Up, typename>
struct rebind : std::__replace_first_arg<_Tp, _Up> {};

template <typename _Tp, typename _Up>
struct rebind<_Tp, _Up, std::void_t<typename _Tp::other>> {
    using type = int;
};

template <typename T>
struct test {
    using result = typename rebind<std::allocator<T>, T, void>::type;
};

)";

using namespace llvm;
using namespace clang;

class Visitor : public clang::RecursiveASTVisitor<Visitor> {
public:
    clang::Sema& sema;
    clang::ASTContext& context;

    Visitor(clang::Sema& sema, clang::ASTContext& context) : sema(sema), context(context) {}

    void instantiateTemplate(clang::ClassTemplateDecl* CTD, llvm::ArrayRef<TemplateArgument> TemplateArgs) {
        void* InsertPos = nullptr;

        if(auto* Spec = CTD->findSpecialization(TemplateArgs, InsertPos)) {
            // return Spec;
        }

        ClassTemplatePartialSpecializationDecl* BestPartialSpec = nullptr;

        clang::sema::TemplateDeductionInfo Info(CTD->getLocation());

        llvm::SmallVector<clang::ClassTemplatePartialSpecializationDecl*> partials;
        CTD->getPartialSpecializations(partials);

        for(auto partial: partials) {
            auto result = sema.DeduceTemplateArguments(partial, TemplateArgs, Info);
            if(result == clang::TemplateDeductionResult::Success) {
                llvm::outs() << "success\n";
                partial->dump();
                for(auto& arg: Info.takeSugared()->asArray()) {
                    llvm::outs() << "---------------------------------------\n";
                    arg.dump();
                }
            }
        }

        // return cast<ClassTemplateSpecializationDecl>(CTD->getTemplatedDecl());
    }

    bool VisitTemplateSpecializationType(const clang::TemplateSpecializationType* type) {

        if(auto CTD = llvm::dyn_cast<clang::ClassTemplateDecl>(type->getTemplateName().getAsTemplateDecl())) {
            if(CTD->getName() == "rebind") {
                llvm::SmallVector<clang::TemplateArgument> arguments;
                llvm::outs() << "count: " << type->template_arguments().size() << "\n";
                for(auto arg: type->template_arguments()) {
                    if(arg.getKind() == clang::TemplateArgument::ArgKind::Type) {
                        arguments.emplace_back(arg.getAsType().getCanonicalType());
                    } else {
                        arguments.emplace_back(arg);
                    }
                }
                instantiateTemplate(CTD, arguments);
            }
        }

        return true;
    }
};

int main(int argc, const char** argv) {
    // clice::execute_path = argv[0];
    auto args = std::vector<const char*>{
        "/usr/bin/c++",
        "main.cpp",
        "-resource-dir",
        "/home/ykiko/C++/clice2/build/lib/clang/20",
    };
    auto preamble = clice::Preamble::build("main.cpp", source, args);
    auto parsedAST = clice::ParsedAST::build("main.cpp", source, args, preamble.get());
    Visitor visitor(parsedAST->sema, parsedAST->context);
    auto tu = parsedAST->context.getTranslationUnitDecl();
    // tu->dump();
    visitor.TraverseDecl(parsedAST->context.getTranslationUnitDecl());
}
