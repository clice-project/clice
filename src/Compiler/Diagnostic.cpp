#include <Support/Support.h>
#include <Compiler/Diagnostic.h>

#include <clang/Basic/DiagnosticIDs.h>
#include <clang/Basic/AllDiagnostics.h>
#include <exception>

// #include <spdlog/fmt/bundled/color.h>

namespace clice {

void DiagnosticCollector::BeginSourceFile(const clang::LangOptions& Opts, const clang::Preprocessor* PP) {

};

const char* getDiagnosticCode(unsigned ID) {
    switch(ID) {
#define DIAG(ENUM,                                                                                                     \
             CLASS,                                                                                                    \
             DEFAULT_MAPPING,                                                                                          \
             DESC,                                                                                                     \
             GROPU,                                                                                                    \
             SFINAE,                                                                                                   \
             NOWERROR,                                                                                                 \
             SHOWINSYSHEADER,                                                                                          \
             SHOWINSYSMACRO,                                                                                           \
             DEFERRABLE,                                                                                               \
             CATEGORY)                                                                                                 \
    case clang::diag::ENUM: return #ENUM;
#include "clang/Basic/DiagnosticASTKinds.inc"
#include "clang/Basic/DiagnosticAnalysisKinds.inc"
#include "clang/Basic/DiagnosticCommentKinds.inc"
#include "clang/Basic/DiagnosticCommonKinds.inc"
#include "clang/Basic/DiagnosticDriverKinds.inc"
#include "clang/Basic/DiagnosticFrontendKinds.inc"
#include "clang/Basic/DiagnosticLexKinds.inc"
#include "clang/Basic/DiagnosticParseKinds.inc"
#include "clang/Basic/DiagnosticRefactoringKinds.inc"
#include "clang/Basic/DiagnosticSemaKinds.inc"
#include "clang/Basic/DiagnosticSerializationKinds.inc"
#undef DIAG
        default: return nullptr;
    }
}

// see llvm/clang/include/clang/AST/ASTDiagnostic.h
void dumpArg(clang::DiagnosticsEngine::ArgumentKind kind, std::uint64_t value) {
    switch(kind) {
        case clang::DiagnosticsEngine::ak_identifierinfo: {
            clang::IdentifierInfo* info = reinterpret_cast<clang::IdentifierInfo*>(value);
            llvm::outs() << info->getName();
            break;
        }

        case clang::DiagnosticsEngine::ak_qual: {
            clang::Qualifiers qual = clang::Qualifiers::fromOpaqueValue(value);
            llvm::outs() << qual.getAsString();
            break;
        }

        case clang::DiagnosticsEngine::ak_qualtype: {
            clang::QualType type = clang::QualType::getFromOpaquePtr(reinterpret_cast<void*>(value));
            llvm::outs() << type.getAsString();
            break;
        }

        case clang::DiagnosticsEngine::ak_qualtype_pair: {
            clang::TemplateDiffTypes& TDT = *reinterpret_cast<clang::TemplateDiffTypes*>(value);
            clang::QualType type1 = clang::QualType::getFromOpaquePtr(reinterpret_cast<void*>(TDT.FromType));
            clang::QualType type2 = clang::QualType::getFromOpaquePtr(reinterpret_cast<void*>(TDT.ToType));
            llvm::outs() << type1.getAsString() << " -> " << type2.getAsString();
            break;
        }

        case clang::DiagnosticsEngine::ak_declarationname: {
            clang::DeclarationName name = clang::DeclarationName::getFromOpaqueInteger(value);
            llvm::outs() << name.getAsString();
            break;
        }

        case clang::DiagnosticsEngine::ak_nameddecl: {
            clang::NamedDecl* decl = reinterpret_cast<clang::NamedDecl*>(value);
            llvm::outs() << decl->getNameAsString();
            break;
        }

        case clang::DiagnosticsEngine::ak_nestednamespec: {
            clang::NestedNameSpecifier* spec = reinterpret_cast<clang::NestedNameSpecifier*>(value);
            spec->dump();
            break;
        }

        case clang::DiagnosticsEngine::ak_declcontext: {
            clang::DeclContext* context = reinterpret_cast<clang::DeclContext*>(value);
            llvm::outs() << context->getDeclKindName();
            break;
        }

        case clang::DiagnosticsEngine::ak_attr: {
            clang::Attr* attr = reinterpret_cast<clang::Attr*>(value);
            break;
            // attr->dump();
        }

        default: {
            std::terminate();
        }
    }

    llvm::outs() << "\n";
}

void DiagnosticCollector::HandleDiagnostic(clang::DiagnosticsEngine::Level level, const clang::Diagnostic& diagnostic) {
    llvm::SmallString<128> message;
    diagnostic.FormatDiagnostic(message);
    // diagnostic.getLocation();
    // fmt::print(fg(fmt::color::red),
    //           "[Diagnostic, kind: {}, message: {}]\n",
    //           refl::enum_name(level),
    //           message.str().str());
    diagnostic.getLocation().dump(diagnostic.getDiags()->getSourceManager());
    // get diagnostic text.
    auto id = diagnostic.getID();
    llvm::outs() << getDiagnosticCode(id) << "\n";
    // llvm::outs() << diagnostic.getDiags()->getDiagnosticIDs()->getDescription(id) << "\n";

    // dumpArg(diagnostic.getArgKind(0), diagnostic.getRawArg(0));

    // FIXME:
    // use DiagnosticEngine::SetArgToStringFn to set a custom function to convert arguments to strings.
    // Support markdown diagnostic in LSP 3.18. allow complex type to display in markdown code block.
};

void DiagnosticCollector::EndSourceFile() {

};

}  // namespace clice
