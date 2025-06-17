#include "Compiler/Diagnostic.h"
#include "clang/AST/Type.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticIDs.h"
#include "clang/Basic/AllDiagnostics.h"
#include "clang/Basic/SourceManager.h"

namespace clice {

llvm::StringRef Diagnostic::diagnostic_code(std::uint32_t ID) {
    switch(ID) {
#define DIAG(ENUM,                                                                                 \
             CLASS,                                                                                \
             DEFAULT_MAPPING,                                                                      \
             DESC,                                                                                 \
             GROPU,                                                                                \
             SFINAE,                                                                               \
             NOWERROR,                                                                             \
             SHOWINSYSHEADER,                                                                      \
             SHOWINSYSMACRO,                                                                       \
             DEFERRABLE,                                                                           \
             CATEGORY)                                                                             \
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
        default: return llvm::StringRef();
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
            clang::QualType type =
                clang::QualType::getFromOpaquePtr(reinterpret_cast<void*>(value));
            llvm::outs() << type.getAsString();
            break;
        }

        case clang::DiagnosticsEngine::ak_qualtype_pair: {
            clang::TemplateDiffTypes& TDT = *reinterpret_cast<clang::TemplateDiffTypes*>(value);
            clang::QualType type1 =
                clang::QualType::getFromOpaquePtr(reinterpret_cast<void*>(TDT.FromType));
            clang::QualType type2 =
                clang::QualType::getFromOpaquePtr(reinterpret_cast<void*>(TDT.ToType));
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
            std::abort();
        }
    }

    llvm::outs() << "\n";
}

// Checks whether a location is within a half-open range.
// Note that clang also uses closed source ranges, which this can't handle!
bool locationInRange(clang::SourceLocation L,
                     clang::CharSourceRange R,
                     const clang::SourceManager& M) {
    assert(R.isCharRange());
    if(!R.isValid() || M.getFileID(R.getBegin()) != M.getFileID(R.getEnd()) ||
       M.getFileID(R.getBegin()) != M.getFileID(L))
        return false;
    return L != R.getEnd() && M.isPointWithin(L, R.getBegin(), R.getEnd());
}

class DiagnosticCollector : public clang::DiagnosticConsumer {
public:
    DiagnosticCollector(std::shared_ptr<std::vector<Diagnostic>> diagnostics) :
        diagnostics(diagnostics) {}

    static DiagnosticLevel diagnostic_level(clang::DiagnosticsEngine::Level level) {
        switch(level) {
            case clang::DiagnosticsEngine::Ignored: return DiagnosticLevel::Ignored;
            case clang::DiagnosticsEngine::Note: return DiagnosticLevel::Note;
            case clang::DiagnosticsEngine::Remark: return DiagnosticLevel::Remark;
            case clang::DiagnosticsEngine::Warning: return DiagnosticLevel::Warning;
            case clang::DiagnosticsEngine::Error: return DiagnosticLevel::Error;
            case clang::DiagnosticsEngine::Fatal: return DiagnosticLevel::Fatal;
            default: return DiagnosticLevel::Invalid;
        }
    }

    void BeginSourceFile(const clang::LangOptions& Opts, const clang::Preprocessor* PP) override {}

    void HandleDiagnostic(clang::DiagnosticsEngine::Level level,
                          const clang::Diagnostic& raw_diagnostic) override {

        auto& diagnostic = diagnostics->emplace_back();
        diagnostic.id = raw_diagnostic.getID();
        diagnostic.level = diagnostic_level(level);

        llvm::SmallString<256> message;
        raw_diagnostic.FormatDiagnostic(message);
        diagnostic.message = message.str();

        auto location = raw_diagnostic.getLocation();
        if(location.isInvalid()) {
            return;
        }

        auto& SM = raw_diagnostic.getDiags()->getSourceManager();
        for(auto& range: raw_diagnostic.getRanges()) {
            if(locationInRange(raw_diagnostic.getLocation(), range, SM)) {
                diagnostic.range = range.getAsRange();
                break;
            }
        }

        // TODO:
        // use DiagnosticEngine::SetArgToStringFn to set a custom function to convert arguments to
        // strings. Support markdown diagnostic in LSP 3.18. allow complex type to display in
        // markdown code block.
    }

    void EndSourceFile() override {}

private:
    std::shared_ptr<std::vector<Diagnostic>> diagnostics;
};

clang::DiagnosticConsumer*
    Diagnostic::create(std::shared_ptr<std::vector<Diagnostic>> diagnostics) {
    return new DiagnosticCollector(diagnostics);
}

}  // namespace clice
