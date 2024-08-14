#include <Clang/Directive.h>

namespace clice {

class DirectiveCollector : public PPCallbacks {
private:
    Directive& directive;

public:
    DirectiveCollector(Directive& directive) : directive(directive) {}

    virtual ~DirectiveCollector() = default;

    void InclusionDirective(SourceLocation HashLoc,
                            const Token& IncludeTok,
                            StringRef FileName,
                            bool IsAngled,
                            clang::CharSourceRange FilenameRange,
                            clang::OptionalFileEntryRef File,
                            StringRef SearchPath,
                            StringRef RelativePath,
                            const clang::Module* SuggestedModule,
                            bool ModuleImported,
                            clang::SrcMgr::CharacteristicKind FileType) override {}

    void moduleImport(SourceLocation ImportLoc,
                      clang::ModuleIdPath Path,
                      const clang::Module* Imported) override {}

    void PragmaDirective(SourceLocation Loc, clang::PragmaIntroducerKind Introducer) override {
        directive.pragmas.emplace_back(Loc, Introducer);
    }

    void MacroDefined(const Token& MacroNameTok, const clang::MacroDirective* MD) override {
        directive.defines.emplace_back(MacroNameTok, MD);
    }

    void MacroExpands(const Token& MacroNameTok,
                      const clang::MacroDefinition& MD,
                      SourceRange Range,
                      const clang::MacroArgs* Args) override {}

    void MacroUndefined(const Token& MacroNameTok,
                        const clang::MacroDefinition& MD,
                        const clang::MacroDirective* Undef) override {
        directive.undefs.emplace_back(MacroNameTok, &MD, Undef);
    }

    void If(SourceLocation Loc,
            SourceRange ConditionRange,
            ConditionValueKind ConditionValue) override {
        directive.ifBlocks.emplace_back(Directive::If{
            Loc,
            {ConditionRange, ConditionValue}
        });
    }

    void Elif(SourceLocation Loc,
              SourceRange ConditionRange,
              ConditionValueKind ConditionValue,
              SourceLocation IfLoc) override {
        assert(directive.ifBlocks.back().if_.location == IfLoc &&
               "The `#elif` directive must be preceded by an `#if` directive.");
        directive.ifBlocks.back().elifs.emplace_back(Directive::Elif{
            Loc,
            {ConditionRange, ConditionValue}
        });
    }

    void Ifdef(SourceLocation Loc,
               const Token& MacroNameTok,
               const clang::MacroDefinition& MD) override {
        directive.ifdefBlocks.emplace_back(Directive::Ifdef{
            Loc,
            {MacroNameTok, MD}
        });
    }

    // call when a `#elifdef` directive is skipped.
    void Elifdef(SourceLocation Loc, SourceRange ConditionRange, SourceLocation IfLoc) override {
        assert(directive.ifdefBlocks.back().ifdef.location == IfLoc &&
               "The `#elifdef` directive must be preceded by an `#ifdef` directive.");
        // FIXME:
        directive.ifdefBlocks.back().elifdefs.emplace_back(Directive::Elifdef{
            Loc,
            //{ConditionRange, ConditionValueKind::Defined}
        });
    }

    // call when a `#ifdef` directive is hit.
    void Elifdef(SourceLocation Loc,
                 const Token& MacroNameTok,
                 const clang::MacroDefinition& MD) override {
        directive.ifdefBlocks.back().elifdefs.emplace_back(Directive::Elifdef{
            Loc,
            {MacroNameTok, MD}
        });
    }

    void Ifndef(SourceLocation Loc,
                const Token& MacroNameTok,
                const clang::MacroDefinition& MD) override {}

    void Elifndef(SourceLocation Loc,
                  const Token& MacroNameTok,
                  const clang::MacroDefinition& MD) override {}

    void Else(SourceLocation Loc, SourceLocation IfLoc) override {}

    void Endif(SourceLocation Loc, SourceLocation IfLoc) override {}
};

Directive::Directive(clang::Preprocessor& preprocessor) {
    preprocessor.addPPCallbacks(std::make_unique<DirectiveCollector>(*this));
}

}  // namespace clice
