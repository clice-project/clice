#include <Compiler/Directive.h>
#include <Support/Reflection.h>
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

struct PPCallback : clang::PPCallbacks {
    PPCallback(Directives& directive) : directive(directive) {}

    Directives& directive;
    clang::FileID currentID;

    void FileChanged(clang::SourceLocation loc,
                     clang::PPCallbacks::FileChangeReason reason,
                     clang::SrcMgr::CharacteristicKind fileType,
                     clang::FileID) override {
        // llvm::outs() << directive.sourceManager.getFileEntryForID(id)->tryGetRealPathName();
        if(reason == clang::PPCallbacks::FileChangeReason::EnterFile) {
            currentID = directive.sourceManager.getFileID(loc);
        }
    }

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

    void PragmaDirective(clang::SourceLocation Loc, clang::PragmaIntroducerKind Introducer) override {
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

    void Ifdef(clang::SourceLocation loc, const clang::Token& name, const clang::MacroDefinition& definition) override {
    }

    void Elifdef(clang::SourceLocation loc,
                 const clang::Token& name,
                 const clang::MacroDefinition& definition) override {}

    void Elifdef(clang::SourceLocation loc, clang::SourceRange conditionRange, clang::SourceLocation ifLoc) override {}

    void Ifndef(clang::SourceLocation loc,
                const clang::Token& name,
                const clang::MacroDefinition& definition) override {}

    // invoke when #elifndef is taken
    void Elifndef(clang::SourceLocation loc,
                  const clang::Token& name,
                  const clang::MacroDefinition& definition) override {}

    // invoke when #elifndef is skipped
    void Elifndef(clang::SourceLocation loc, clang::SourceRange conditionRange, clang::SourceLocation ifLoc) override {}

    void Else(clang::SourceLocation loc, clang::SourceLocation ifLoc) override {}

    void Endif(clang::SourceLocation loc, clang::SourceLocation ifLoc) override {}

    void MacroDefined(const clang::Token& MacroNameTok, const clang::MacroDirective* MD) override {}

    void MacroExpands(const clang::Token& MacroNameTok,
                      const clang::MacroDefinition& MD,
                      clang::SourceRange Range,
                      const clang::MacroArgs* Args) override {
        // llvm::outs() << "------------------------\n";
        // auto info = MD.getMacroInfo();
        // if(info->isObjectLike()) {
        //     Range = clang::SourceRange(MacroNameTok.getLocation(), MacroNameTok.getEndLoc());
        // } else if(info->isFunctionLike()) {
        //     // MacroNameTok.getLocation().dump(directive.sourceManager);
        // }
        // Range.getBegin().dump(directive.sourceManager);
        // Range.getEnd().dump(directive.sourceManager);
        // auto s = directive.sourceManager.getCharacterData(Range.getBegin());
        // auto e = directive.sourceManager.getCharacterData(Range.getEnd());
        //// llvm::outs() << llvm::StringRef(s, e - s) << "\n";
        //
        // llvm::outs() << directive.preproc.getSpelling(MacroNameTok) << " ";
        // if(Args) {
        //    auto size = Args->getNumMacroArguments();
        //    for(int i = 0; i < size; i++) {
        //        auto arg = Args->getUnexpArgument(i);
        //        auto len = Args->getArgLength(arg);
        //        for(int j = 0; j < len; j++) {
        //            llvm::outs() << directive.preproc.getSpelling(arg[j]) << " ";
        //        }
        //    }
        //}
        // llvm::outs() << "\n";
    }
};

clang::CommentHandler* Directives::handler() { return new CommentHandler(*this); }

std::unique_ptr<clang::PPCallbacks> Directives::callback() { return std::make_unique<PPCallback>(*this); }

}  // namespace clice
