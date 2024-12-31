#include <Support/Support.h>
#include <Compiler/Directive.h>
#include <clang/Lex/MacroInfo.h>
#include <clang/Lex/MacroArgs.h>

namespace clice {

namespace {

class PPCallback : public clang::PPCallbacks {
public:
    PPCallback(clang::Preprocessor& pp, llvm::DenseMap<clang::FileID, Directive>& directives) :
        pp(pp), directives(directives) {}

private:
    void addCondition(clang::SourceLocation loc,
                      Condition::BranchKind kind,
                      Condition::ConditionValue value,
                      clang::SourceRange conditionRange) {
        auto& directive = directives[pp.getSourceManager().getFileID(loc)];
        directive.conditions.emplace_back(Condition{kind, value, loc, conditionRange});
    }

    void addCondition(clang::SourceLocation loc,
                      Condition::BranchKind kind,
                      clang::PPCallbacks::ConditionValueKind value,
                      clang::SourceRange conditionRange) {
        Condition::ConditionValue condValue = Condition::None;
        switch(value) {
            case clang::PPCallbacks::CVK_False: {
                condValue = Condition::False;
                break;
            }
            case clang::PPCallbacks::CVK_True: {
                condValue = Condition::True;
                break;
            }
            case clang::PPCallbacks::CVK_NotEvaluated: {
                condValue = Condition::Skipped;
                break;
            }
        }
        addCondition(loc, kind, condValue, conditionRange);
    }

    void addCondition(clang::SourceLocation loc,
                      Condition::BranchKind kind,
                      const clang::Token& name,
                      const clang::MacroDefinition& definition) {
        if(auto def = definition.getMacroInfo()) {
            addMacro(def, MacroRef::Ref, name.getLocation());
            addCondition(loc, kind, Condition::True, name.getLocation());
        } else {
            addCondition(loc, kind, Condition::False, name.getLocation());
        }
    }

    void addMacro(const clang::MacroInfo* def, MacroRef::Kind kind, clang::SourceLocation loc) {
        if(pp.getSourceManager().isWrittenInBuiltinFile(loc) ||
           pp.getSourceManager().isWrittenInCommandLineFile(loc)) {
            return;
        }

        auto& directive = directives[pp.getSourceManager().getFileID(loc)];
        directive.macros.emplace_back(MacroRef{def, kind, loc});
    }

private:
    void LexedFileChanged(clang::FileID currID,
                          LexedFileChangeReason reason,
                          clang::SrcMgr::CharacteristicKind kind,
                          clang::FileID prevID,
                          clang::SourceLocation location) override {
        /// If prevID is invalid, it means we are entering main file
        /// or builtin files. Skip it.
        if(this->prevID.isInvalid() || prevID.isInvalid()) {
            return;
        }

        if(reason == LexedFileChangeReason::EnterFile) {
            assert(this->prevID == prevID && "Unexpected file change");
            assert(directives.contains(prevID) && "Unexpected file change");
            assert(directives[prevID].includes.back().fid.isInvalid() && "Unexpected filechange");
            directives[prevID].includes.back().fid = currID;
        }
    }

    void InclusionDirective(clang::SourceLocation hashLoc,
                            const clang::Token& includeTok,
                            llvm::StringRef spellingFilename,
                            bool isAngled,
                            clang::CharSourceRange filenameRange,
                            clang::OptionalFileEntryRef file,
                            llvm::StringRef searchPath,
                            llvm::StringRef relativePath,
                            const clang::Module* suggestedModule,
                            bool moduleImported,
                            clang::SrcMgr::CharacteristicKind fileType) override {
        /// When see a `#include` directive, we are going to change file.
        prevID = pp.getSourceManager().getFileID(hashLoc);
        directives[prevID].includes.emplace_back(Include{
            .include = includeTok.getLocation(),
            /// `fid` is assigned in `LexedFileChanged`. Note that if the file is skipped
            /// because of header guard optimization, the `LexedFileChanged` will not called
            /// after `InclusionDirective`. Then `fid` will be invalid.
            .fid = {},
            .path = file->getName(),
            .range = filenameRange.getAsRange(),
        });
    }

    void PragmaDirective(clang::SourceLocation Loc,
                         clang::PragmaIntroducerKind Introducer) override {}

    void If(clang::SourceLocation loc,
            clang::SourceRange conditionRange,
            clang::PPCallbacks::ConditionValueKind value) override {
        addCondition(loc, Condition::If, value, conditionRange);
    }

    void Elif(clang::SourceLocation loc,
              clang::SourceRange conditionRange,
              clang::PPCallbacks::ConditionValueKind value,
              clang::SourceLocation) override {
        addCondition(loc, Condition::Elif, value, conditionRange);
    }

    void Ifdef(clang::SourceLocation loc,
               const clang::Token& name,
               const clang::MacroDefinition& definition) override {
        addCondition(loc, Condition::Ifdef, name, definition);
    }

    /// Invoke when #elifdef branch is taken.
    void Elifdef(clang::SourceLocation loc,
                 const clang::Token& name,
                 const clang::MacroDefinition& definition) override {
        addCondition(loc, Condition::Elifdef, name, definition);
    }

    /// Invoke when #elif is skipped.
    void Elifdef(clang::SourceLocation loc,
                 clang::SourceRange conditionRange,
                 clang::SourceLocation) override {
        /// FIXME: should we try to evaluate the condition to compute the macro reference?
        addCondition(loc, Condition::Elifdef, Condition::Skipped, conditionRange);
    }

    /// Invoke when #ifndef is taken.
    void Ifndef(clang::SourceLocation loc,
                const clang::Token& name,
                const clang::MacroDefinition& definition) override {
        addCondition(loc, Condition::Ifndef, name, definition);
    }

    // Invoke when #elifndef is taken.
    void Elifndef(clang::SourceLocation loc,
                  const clang::Token& name,
                  const clang::MacroDefinition& definition) override {
        addCondition(loc, Condition::Elifndef, name, definition);
    }

    // Invoke when #elifndef is skipped.
    void Elifndef(clang::SourceLocation loc,
                  clang::SourceRange conditionRange,
                  clang::SourceLocation) override {
        addCondition(loc, Condition::Elifndef, Condition::Skipped, conditionRange);
    }

    void Else(clang::SourceLocation loc, clang::SourceLocation ifLoc) override {
        addCondition(loc, Condition::Else, Condition::None, clang::SourceRange());
    }

    void Endif(clang::SourceLocation loc, clang::SourceLocation ifLoc) override {
        addCondition(loc, Condition::EndIf, Condition::None, clang::SourceRange());
    }

    void MacroDefined(const clang::Token& name, const clang::MacroDirective* MD) override {
        if(auto def = MD->getMacroInfo()) {
            addMacro(def, MacroRef::Def, name.getLocation());
        }
    }

    void MacroExpands(const clang::Token& name,
                      const clang::MacroDefinition& definition,
                      clang::SourceRange range,
                      const clang::MacroArgs* args) override {
        if(auto def = definition.getMacroInfo()) {
            addMacro(def, MacroRef::Ref, name.getLocation());
        }
    }

    void MacroUndefined(const clang::Token& name,
                        const clang::MacroDefinition& MD,
                        const clang::MacroDirective* undef) override {
        if(auto def = MD.getMacroInfo()) {
            addMacro(def, MacroRef::Undef, name.getLocation());
        }
    }

private:
    clang::FileID prevID;
    clang::Preprocessor& pp;
    llvm::DenseMap<clang::FileID, Directive>& directives;
    llvm::DenseMap<clang::MacroInfo*, std::size_t> macroCache;
};

}  // namespace

void Directive::attach(clang::Preprocessor& pp,
                       llvm::DenseMap<clang::FileID, Directive>& directives) {
    pp.addPPCallbacks(std::make_unique<PPCallback>(pp, directives));
}

}  // namespace clice
