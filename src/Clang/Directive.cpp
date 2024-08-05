#include <Clang/Directive.h>
#include <vector>

namespace clice {

class DirectiveCollector : clang::PPCallbacks {
public:
    virtual ~DirectiveCollector() = default;

    void InclusionDirective(clang::SourceLocation HashLoc,
                            const clang::Token& IncludeTok,
                            StringRef FileName,
                            bool IsAngled,
                            clang::CharSourceRange FilenameRange,
                            clang::OptionalFileEntryRef File,
                            StringRef SearchPath,
                            StringRef RelativePath,
                            const clang::Module* SuggestedModule,
                            bool ModuleImported,
                            clang::SrcMgr::CharacteristicKind FileType) override {}

    void moduleImport(clang::SourceLocation ImportLoc,
                      clang::ModuleIdPath Path,
                      const clang::Module* Imported) override {}

    void PragmaDirective(clang::SourceLocation Loc, clang::PragmaIntroducerKind Introducer) override {}

    void MacroExpands(const clang::Token& MacroNameTok,
                      const clang::MacroDefinition& MD,
                      clang::SourceRange Range,
                      const clang::MacroArgs* Args) override {}

    void MacroDefined(const clang::Token& MacroNameTok, const clang::MacroDirective* MD) override {}

    void MacroUndefined(const clang::Token& MacroNameTok,
                        const clang::MacroDefinition& MD,
                        const clang::MacroDirective* Undef) override {}

    void If(clang::SourceLocation Loc,
            clang::SourceRange ConditionRange,
            ConditionValueKind ConditionValue) override {}

    void Elif(clang::SourceLocation Loc,
              clang::SourceRange ConditionRange,
              ConditionValueKind ConditionValue,
              clang::SourceLocation IfLoc) override {}

    void Ifdef(clang::SourceLocation Loc,
               const clang::Token& MacroNameTok,
               const clang::MacroDefinition& MD) override {}

    void Elifdef(clang::SourceLocation Loc,
                 const clang::Token& MacroNameTok,
                 const clang::MacroDefinition& MD) override {}

    void Ifndef(clang::SourceLocation Loc,
                const clang::Token& MacroNameTok,
                const clang::MacroDefinition& MD) override {}

    void Elifndef(clang::SourceLocation Loc,
                  const clang::Token& MacroNameTok,
                  const clang::MacroDefinition& MD) override {}

    void Else(clang::SourceLocation Loc, clang::SourceLocation IfLoc) override {}

    void Endif(clang::SourceLocation Loc, clang::SourceLocation IfLoc) override {}
};

}  // namespace clice
