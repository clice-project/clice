#include <Support/Support.h>
#include <Compiler/Directive.h>
#include <clang/Lex/MacroInfo.h>
#include <clang/Lex/MacroArgs.h>

namespace clice {

struct CommentHandler : public clang::CommentHandler {
    Directives& directive;

    CommentHandler(Directives& directive) : directive(directive) {}

    virtual bool HandleComment(clang::Preprocessor& preproc, clang::SourceRange Comment) {
        // directive.comments.push_back(Comment);
        auto start = directive.sourceManager.getCharacterData(Comment.getBegin());
        auto end = directive.sourceManager.getCharacterData(Comment.getEnd());
        // llvm::outs() << "Comment: " << llvm::StringRef(start, end - start) << "\n";
        return false;
    }
};

struct PragmaHandler : public clang::PragmaHandler {
    virtual void HandlePragma(clang::Preprocessor& PP,
                              clang::PragmaIntroducer Introducer,
                              clang::Token& FirstToken) {
        // TODO:
    }

    /// getIfNamespace - If this is a namespace, return it.  This is equivalent to
    /// using a dynamic_cast, but doesn't require RTTI.
    virtual clang::PragmaNamespace* getIfNamespace() {
        return nullptr;
    }
};

struct PPCallback : clang::PPCallbacks {
    PPCallback(clang::Preprocessor& preproc) :
        preproc(preproc), srcMgr(preproc.getSourceManager()) {}

    clang::Preprocessor& preproc;
    clang::SourceManager& srcMgr;
    ;
    clang::FileID currentID;

    void FileChanged(clang::SourceLocation loc,
                     clang::PPCallbacks::FileChangeReason reason,
                     clang::SrcMgr::CharacteristicKind fileType,
                     clang::FileID) override {}

    void InclusionDirective(clang::SourceLocation HashLoc,
                            const clang::Token& IncludeTok,
                            llvm::StringRef FileName,
                            bool IsAngled,
                            clang::CharSourceRange FilenameRange,
                            clang::OptionalFileEntryRef File,
                            clang::StringRef SearchPath,
                            llvm::StringRef RelativePath,
                            const clang::Module* SuggestedModule,
                            bool ModuleImported,
                            clang::SrcMgr::CharacteristicKind FileType) override {
        // TODO: record all include files
        llvm::SmallVector<char> RealPath;
        // fs::make_absolute(SearchPath + "/" + RelativePath, RealPath);
        // path::remove_dots(RealPath, /*remove_dot_dot=*/true);
        // llvm::outs() << RealPath << "\n";
    }

    void PragmaDirective(clang::SourceLocation Loc,
                         clang::PragmaIntroducerKind Introducer) override {
        // llvm::outs() << "PragmaDirective\n";
    }

    void If(clang::SourceLocation Loc,
            clang::SourceRange ConditionRange,
            clang::PPCallbacks::ConditionValueKind ConditionValue) override {
        // llvm::outs() << "If\n";
    }

    void Elif(clang::SourceLocation loc,
              clang::SourceRange conditionRange,
              clang::PPCallbacks::ConditionValueKind conditionValue,
              clang::SourceLocation ifLoc) override {}

    void Ifdef(clang::SourceLocation loc,
               const clang::Token& name,
               const clang::MacroDefinition& definition) override {}

    void Elifdef(clang::SourceLocation loc,
                 const clang::Token& name,
                 const clang::MacroDefinition& definition) override {}

    void Elifdef(clang::SourceLocation loc,
                 clang::SourceRange conditionRange,
                 clang::SourceLocation ifLoc) override {}

    void Ifndef(clang::SourceLocation loc,
                const clang::Token& name,
                const clang::MacroDefinition& definition) override {}

    // invoke when #elifndef is taken
    void Elifndef(clang::SourceLocation loc,
                  const clang::Token& name,
                  const clang::MacroDefinition& definition) override {}

    // invoke when #elifndef is skipped
    void Elifndef(clang::SourceLocation loc,
                  clang::SourceRange conditionRange,
                  clang::SourceLocation ifLoc) override {}

    void Else(clang::SourceLocation loc, clang::SourceLocation ifLoc) override {}

    void Endif(clang::SourceLocation loc, clang::SourceLocation ifLoc) override {}

    void MacroDefined(const clang::Token& MacroNameTok, const clang::MacroDirective* MD) override {
        if(MD) {
            auto info = MD->getMacroInfo();
            // llvm::outs() << "is builtin: " << info->isBuiltinMacro() << "\n";
            if(!info->isBuiltinMacro()) {
                auto location = MacroNameTok.getLocation();
                if(!srcMgr.isWrittenInBuiltinFile(location) &&
                   !srcMgr.isWrittenInCommandLineFile(location)) {
                    MacroNameTok.getLocation().dump(srcMgr);
                    // srcMgr.getIncludeLoc(srcMgr.getFileID(MacroNameTok.getLocation())).dump(srcMgr);
                    llvm::outs() << preproc.getSpelling(MacroNameTok) << "\n";
                }
                auto def = MD->getDefinition();
            }
            // MD->dump();
        }
    }

    void MacroExpands(const clang::Token& MacroNameTok,
                      const clang::MacroDefinition& MD,
                      clang::SourceRange Range,
                      const clang::MacroArgs* Args) override {
        auto info = MD.getMacroInfo();
        llvm::outs() << "------------------------\n";
        // auto info = MD.getMacroInfo();
        if(info->isObjectLike()) {
            Range = clang::SourceRange(MacroNameTok.getLocation(), MacroNameTok.getEndLoc());
        } else if(info->isFunctionLike()) {
            // MacroNameTok.getLocation().dump(srcMgr);
        }
        Range.dump(srcMgr);
        auto s = srcMgr.getCharacterData(Range.getBegin());
        auto e = srcMgr.getCharacterData(Range.getEnd());
        llvm::outs() << llvm::StringRef(s, e - s) << "\n";

        llvm::outs() << preproc.getSpelling(MacroNameTok) << " ";
        if(Args) {
            auto size = Args->getNumMacroArguments();
            for(int i = 0; i < size; i++) {
                auto arg = Args->getUnexpArgument(i);
                auto len = Args->getArgLength(arg);
                for(int j = 0; j < len; j++) {
                    llvm::outs() << preproc.getSpelling(arg[j]) << " ";
                }
            }
        }
        llvm::outs() << "\n";
    }

    void MacroUndefined(const clang::Token& MacroNameTok,
                        const clang::MacroDefinition& MD,
                        const clang::MacroDirective* Undef) override {
        // TODO:
    }
};

clang::CommentHandler* Directives::handler() {
    // return new CommentHandler(*this);
}

std::unique_ptr<clang::PPCallbacks> Directives::callback() {
    // return std::make_unique<PPCallback>(*this);
}

}  // namespace clice
