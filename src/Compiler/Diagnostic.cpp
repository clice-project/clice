#include "Compiler/Diagnostic.h"

#include <clang/AST/Type.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/Basic/DiagnosticIDs.h>
#include <clang/Basic/AllDiagnostics.h>
#include <clang/Lex/Preprocessor.h>

namespace clice {

DiagSeverity toSeverity(clang::DiagnosticsEngine::Level level) {
    using Eng = clang::DiagnosticsEngine;
    switch(level) {
        case Eng::Remark: return DiagSeverity::Hint;
        case Eng::Note: return DiagSeverity::Info;
        case Eng::Warning: return DiagSeverity::Warning;
        case Eng::Fatal:
        case Eng::Error: return DiagSeverity::Error;
        case Eng::Ignored: return DiagSeverity::Ignore;
    }
    llvm_unreachable("Unknown diagnostic level!");
}

const char* findDiagName(unsigned ID) {
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
        default: return nullptr;
    }
}

llvm::SmallVector<DiagTag, 2> findDiagTags(unsigned diagID, llvm::StringRef diagName,
                                           DiagSource source) {
    llvm::SmallVector<DiagTag, 2> tags;

    bool hasDep = false, hasUnused = false;

    using namespace clang;
    constexpr std::array DeprecatedDiags = {
        diag::warn_access_decl_deprecated,
        diag::warn_atl_uuid_deprecated,
        diag::warn_deprecated,
        diag::warn_deprecated_altivec_src_compat,
        diag::warn_deprecated_comma_subscript,
        diag::warn_deprecated_copy,
        diag::warn_deprecated_copy_with_dtor,
        diag::warn_deprecated_copy_with_user_provided_copy,
        diag::warn_deprecated_copy_with_user_provided_dtor,
        diag::warn_deprecated_def,
        diag::warn_deprecated_increment_decrement_volatile,
        diag::warn_deprecated_message,
        diag::warn_deprecated_redundant_constexpr_static_def,
        diag::warn_deprecated_register,
        diag::warn_deprecated_simple_assign_volatile,
        diag::warn_deprecated_string_literal_conversion,
        diag::warn_deprecated_this_capture,
        diag::warn_deprecated_volatile_param,
        diag::warn_deprecated_volatile_return,
        diag::warn_deprecated_volatile_structured_binding,
        diag::warn_opencl_attr_deprecated_ignored,
        diag::warn_property_method_deprecated,
        diag::warn_vector_mode_deprecated,
    };

    if(auto it = std::ranges::find(DeprecatedDiags, diagID); it != DeprecatedDiags.end()) {
        tags.push_back(DiagTag::Deprecated);
        hasDep = true;
    }

    constexpr std::array UnusedDiags = {
        diag::warn_opencl_attr_deprecated_ignored,
        diag::warn_pragma_attribute_unused,
        diag::warn_unused_but_set_parameter,
        diag::warn_unused_but_set_variable,
        diag::warn_unused_comparison,
        diag::warn_unused_const_variable,
        diag::warn_unused_exception_param,
        diag::warn_unused_function,
        diag::warn_unused_label,
        diag::warn_unused_lambda_capture,
        diag::warn_unused_local_typedef,
        diag::warn_unused_member_function,
        diag::warn_unused_parameter,
        diag::warn_unused_private_field,
        diag::warn_unused_property_backing_ivar,
        diag::warn_unused_template,
        diag::warn_unused_variable,
    };

    if(auto it = std::ranges::find(UnusedDiags, diagID); it != UnusedDiags.end()) {
        tags.push_back(DiagTag::Unnecessary);
        hasUnused = true;
    }

    if(source == DiagSource::ClangTidy && tags.size() < 2) {
        if(!hasUnused && llvm::StringRef(diagName).starts_with("misc-unused-"))
            tags.push_back(DiagTag::Unnecessary);
        if(!hasDep && llvm::StringRef(diagName).starts_with("modernize-"))
            tags.push_back(DiagTag::Deprecated);
    }

    return tags;
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
            std::terminate();
        }
    }

    llvm::outs() << "\n";
}

clang::SourceRange takeDiagRange(const clang::Diagnostic& diagnostic,
                                 const clang::LangOptions& options) {
    /// TODO:
    /// Fix source range in some cases.
    auto charRange = diagnostic.getRange(0);
    if(charRange.isTokenRange()) {
        return clang::SourceRange(charRange.getBegin(), charRange.getEnd());
    } else {
        return clang::SourceRange(charRange.getBegin(), charRange.getEnd().getLocWithOffset(-1));
    }
}

void DiagnosticCollector::BeginSourceFile(const clang::LangOptions& option,
                                          const clang::Preprocessor* pp) {
    langOpts = &option;
    if(pp)
        originSrcMgr = &pp->getSourceManager();
};

// llvm::SmallString<128> message;
// diagnostic.FormatDiagnostic(message);
// diagnostic.getLocation();
// fmt::print(fg(fmt::color::red),
//           "[Diagnostic, kind: {}, message: {}]\n",
//           refl::enum_name(level),
//           message.str().str());
// diagnostic.getLocation().dump(diagnostic.getDiags()->getSourceManager());
// get diagnostic text.
// auto id = diagnostic.getID();
// llvm::outs() << getDiagnosticCode(id) << "\n";
// llvm::outs() << diagnostic.getDiags()->getDiagnosticIDs()->getDescription(id) << "\n";
// dumpArg(diagnostic.getArgKind(0), diagnostic.getRawArg(0));

llvm::StringRef takeDiagCategoryName(unsigned diagID) {
    unsigned categoryNumber = clang::DiagnosticIDs::getCategoryNumberForDiag(diagID);
    return clang::DiagnosticIDs::getCategoryNameFromID(categoryNumber);
}

/// Put the diagnostic at the start of MainFileID if it has a invalid location.
Diagnostic handleInvalidLocDiag(const clang::LangOptions& options,
                                clang::DiagnosticsEngine::Level level) {
    assert(false && "TODO");
    return {};
}

DiagnosticBase extractDiagnosticBase(const clang::Diagnostic& diagnostic,
                                     const clang::LangOptions& options,
                                     clang::DiagnosticsEngine::Level level) {
    return {
        .range = takeDiagRange(diagnostic, options),

        /// FIXME: check is in main file
        .isInMainFile = true,
        .severity = toSeverity(level),
    };
}

void DiagnosticCollector::HandleDiagnostic(clang::DiagnosticsEngine::Level level,
                                           const clang::Diagnostic& dgsc) {
    assert(this->langOpts != nullptr && "Just check behaviour of clang::DiagnosticConsumer");

    // If the diagnostic was generated for a different SourceManager, skip it.
    // This happens when a module is imported and needs to be implicitly built.
    // The compilation of that module will use the same StoreDiags, but different
    // SourceManager.
    if(originSrcMgr && dgsc.hasSourceManager() && originSrcMgr != &dgsc.getSourceManager())
        return;

    unsigned diagID = dgsc.getID();
    auto location = dgsc.getLocation();

    /// TODO:
    /// Figure out how generate a diagnostic with a invalid location.
    assert(location.isValid());
    // if(location.isInvalid()) {
    //     /// For a diag comes from headers, skip it if not an error.
    //     if(clang::DiagnosticIDs::isDefaultMappingAsError(diagID))
    //         diags.push_back(handleHeaderFileDiagnostic(*langOpts, level));
    //     return;
    // }

    llvm::SmallString<128> message;
    dgsc.FormatDiagnostic(message);

    Diagnostic diag;
    diag.message = message.str().str();
    diag.range = takeDiagRange(dgsc, *langOpts);
    diag.isInMainFile = originSrcMgr->isInMainFile(location);
    diag.source = DiagSource::Clang;
    diag.ID = diagID;
    if(auto* name = findDiagName(diag.ID))
        diag.name = name;
    diag.category = takeDiagCategoryName(diagID);
    diag.severity = toSeverity(level);
    diag.tag = findDiagTags(diagID, diag.name, diag.source);

    diags.push_back(std::move(diag));

    /// TODO:
    /// Impl fix-it

    /// TODO:
    /// Collect notes

    /// FIXME:
    /// use DiagnosticEngine::SetArgToStringFn to set a custom function to convert arguments to
    /// strings. Support markdown diagnostic in LSP 3.18. allow complex type to display in markdown
    /// code block.
};

void DiagnosticCollector::EndSourceFile() {
    langOpts = nullptr;
    originSrcMgr = nullptr;
};

auto DiagnosticCollector::takeWithTidyContext(const clang::tidy::ClangTidyContext* tidy)
    -> std::vector<Diagnostic> {
    assert(tidy == nullptr && "Don't supoort tidy now");
    return std::move(this->diags);
}

}  // namespace clice
