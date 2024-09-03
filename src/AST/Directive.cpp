#include "AST/Directive.h"

namespace clice {

struct CommentHandler : public clang::CommentHandler {
    Directive& directive;

    CommentHandler(Directive& directive) : directive(directive) {}

    virtual bool HandleComment(clang::Preprocessor& preproc, clang::SourceRange Comment) {
        directive.comments.push_back(Comment);
        return false;
    }
};

struct PPCallback : clang::PPCallbacks {
    virtual void InclusionDirective(clang::SourceLocation HashLoc,
                                    const clang::Token& IncludeTok,
                                    llvm::StringRef FileName,
                                    bool IsAngled,
                                    clang::CharSourceRange FilenameRange,
                                    clang::OptionalFileEntryRef File,
                                    clang::StringRef SearchPath,
                                    llvm::StringRef RelativePath,
                                    const clang::Module* SuggestedModule,
                                    bool ModuleImported,
                                    clang::SrcMgr::CharacteristicKind FileType) {
        // TODO: record all include files
        namespace fs = llvm::sys::fs;
        namespace path = llvm::sys::path;
        llvm::SmallVector<char> RealPath;
        fs::make_absolute(SearchPath + "/" + RelativePath, RealPath);
        path::remove_dots(RealPath, /*remove_dot_dot=*/true);
        llvm::outs() << RealPath << "\n";
    }

    // virtual void
    //     moduleImport(clang::SourceLocation ImportLoc, clang::ModuleIdPath Path, const clang::Module* Imported) {
    //     // store for highlight
    // }
};

clang::CommentHandler* Directive::handler() { return new CommentHandler(*this); }

std::unique_ptr<clang::PPCallbacks> Directive::callback() { return std::make_unique<PPCallback>(); }

}  // namespace clice
