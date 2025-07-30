#include "Compiler/Diagnostic.h"
#include "clang/AST/Type.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticIDs.h"
#include "clang/Basic/AllDiagnostics.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Preprocessor.h"
#include "Support/Format.h"

namespace clice {

llvm::StringRef DiagnosticID::diagnostic_code() {
    switch(value) {
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

std::optional<std::string> DiagnosticID::diagnostic_document_uri() {
    switch(source) {
        case DiagnosticSource::Unknown:
        case DiagnosticSource::Clang: {
            // There is a page listing many warning flags, but it provides too little
            // information to be worth linking.
            // https://clang.llvm.org/docs/DiagnosticsReference.html
            return std::nullopt;
        }

        case DiagnosticSource::ClangTidy: {
            // This won't correctly get the module for clang-analyzer checks, but as we
            // don't link in the analyzer that shouldn't be an issue.
            // This would also need updating if anyone decides to create a module with a
            // '-' in the name.
            auto [module, check] = name.split('-');
            if(module.empty() || check.empty()) {
                return std::nullopt;
            }

            return std::format("https://clang.llvm.org/extra/clang-tidy/checks/{}/{}.html",
                               module,
                               check);
        }

        case DiagnosticSource::Clice: {
            /// TODO: Add diagnostic for clice.
            return std::nullopt;
        }
    }
}

void set_tags(Diagnostic& diagnostic) {
    namespace diag = clang::diag;

    static llvm::DenseSet<std::uint32_t> deprecated_diags{
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

    static llvm::DenseSet<std::uint32_t> unused_diags = {
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

    if(deprecated_diags.contains(diagnostic.id.value)) {
        /// TODO: Add deprecated.
    } else if(unused_diags.contains(diagnostic.id.value)) {
        /// TODO: Add unused.
    }

    /// TODO: see clang tidy.
}

// Checks whether a location is within a half-open range.
// Note that clang also uses closed source ranges, which this can't handle!
bool location_in_range(clang::SourceLocation L,
                       clang::CharSourceRange R,
                       const clang::SourceManager& M) {
    /// assert(R.isCharRange());
    if(!R.isValid() || M.getFileID(R.getBegin()) != M.getFileID(R.getEnd()) ||
       M.getFileID(R.getBegin()) != M.getFileID(L)) {
        return false;
    }

    return L != R.getEnd() && M.isPointWithin(L, R.getBegin(), R.getEnd());
}

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

class DiagnosticCollector : public clang::DiagnosticConsumer {
public:
    DiagnosticCollector(std::shared_ptr<std::vector<Diagnostic>> diagnostics) :
        diagnostics(diagnostics) {}

    void BeginSourceFile(const clang::LangOptions& Opts, const clang::Preprocessor* PP) override {
        src_mgr = &PP->getSourceManager();
    }

    void HandleDiagnostic(clang::DiagnosticsEngine::Level level,
                          const clang::Diagnostic& raw_diagnostic) override {

        auto& diagnostic = diagnostics->emplace_back();
        diagnostic.id.value = raw_diagnostic.getID();
        diagnostic.id.level = diagnostic_level(level);

        /// TODO:
        // use DiagnosticEngine::SetArgToStringFn to set a custom function to convert arguments to
        // strings. Support markdown diagnostic in LSP 3.18. allow complex type to display in
        // markdown code block.
        ///
        /// auto& engine = src_mgr->getDiagnostics();
        /// engine.SetArgToStringFn();

        llvm::SmallString<256> message;
        raw_diagnostic.FormatDiagnostic(message);
        diagnostic.message = message.str();

        auto location = raw_diagnostic.getLocation();
        if(location.isInvalid()) {
            return;
        }

        for(auto& range: raw_diagnostic.getRanges()) {
            if(location_in_range(raw_diagnostic.getLocation(), range, *src_mgr)) {
                diagnostic.range = range.getAsRange();
                break;
            }
        }

        /// TODO: handle FixIt
        /// raw_diagnostic.getFixItHints();
    }

    void EndSourceFile() override {}

private:
    std::shared_ptr<std::vector<Diagnostic>> diagnostics;
    clang::SourceManager* src_mgr;
};

clang::DiagnosticConsumer*
    Diagnostic::create(std::shared_ptr<std::vector<Diagnostic>> diagnostics) {
    return new DiagnosticCollector(diagnostics);
}

}  // namespace clice
