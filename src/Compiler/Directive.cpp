#include <Support/Support.h>
#include <Compiler/Directive.h>
#include <clang/Lex/MacroInfo.h>
#include <clang/Lex/MacroArgs.h>

namespace clice {

namespace {

struct PPCallback : public clang::PPCallbacks {
    clang::FileID prevFID;
    clang::Preprocessor& PP;
    clang::SourceManager& SM;
    llvm::DenseMap<clang::FileID, Directive>& directives;
    llvm::DenseMap<clang::MacroInfo*, std::size_t> macroCache;

    PPCallback(clang::Preprocessor& PP, llvm::DenseMap<clang::FileID, Directive>& directives) :
        PP(PP), SM(PP.getSourceManager()), directives(directives) {}

    void addCondition(clang::SourceLocation loc,
                      Condition::BranchKind kind,
                      Condition::ConditionValue value,
                      clang::SourceRange conditionRange) {
        auto& directive = directives[PP.getSourceManager().getFileID(loc)];
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
        if(def->isBuiltinMacro()) {
            return;
        }

        if(PP.getSourceManager().isWrittenInBuiltinFile(loc) ||
           PP.getSourceManager().isWrittenInCommandLineFile(loc) ||
           PP.getSourceManager().isWrittenInScratchSpace(loc)) {
            return;
        }

        auto& directive = directives[PP.getSourceManager().getFileID(loc)];
        directive.macros.emplace_back(MacroRef{def, kind, loc});
    }

    /// ============================================================================
    ///                         Rewritten Preprocessor Callbacks
    /// ============================================================================

    void LexedFileChanged(clang::FileID currFID,
                          LexedFileChangeReason reason,
                          clang::SrcMgr::CharacteristicKind,
                          clang::FileID prevFID,
                          clang::SourceLocation) override {
        if(reason == LexedFileChangeReason::EnterFile && currFID.isValid() && prevFID.isValid() &&
           this->prevFID.isValid() && prevFID == this->prevFID) {
            directives[prevFID].includes.back().fid = currFID;
        }
    }

    void InclusionDirective(clang::SourceLocation hashLoc,
                            const clang::Token& includeTok,
                            llvm::StringRef,
                            bool,
                            clang::CharSourceRange,
                            clang::OptionalFileEntryRef,
                            llvm::StringRef,
                            llvm::StringRef,
                            const clang::Module*,
                            bool,
                            clang::SrcMgr::CharacteristicKind) override {
        prevFID = SM.getFileID(hashLoc);
        directives[prevFID].includes.emplace_back(Include{
            {},
            includeTok.getLocation(),
        });
    }

    void HasInclude(clang::SourceLocation location,
                    llvm::StringRef,
                    bool,
                    clang::OptionalFileEntryRef file,
                    clang::SrcMgr::CharacteristicKind) override {
        directives[SM.getFileID(location)].hasIncludes.emplace_back(clice::HasInclude{
            file ? file->getName() : "",
            location,
        });
    }

    void PragmaDirective(clang::SourceLocation Loc,
                         clang::PragmaIntroducerKind Introducer) override {
        // Ignore other cases except starts with `#pragma`.
        if(Introducer != clang::PragmaIntroducerKind::PIK_HashPragma)
            return;

        clang::FileID fid = SM.getFileID(Loc);

        llvm::StringRef textToEnd = SM.getBufferData(fid).substr(SM.getFileOffset(Loc));
        llvm::StringRef thatLine = textToEnd.take_until([](char ch) { return ch == '\n'; });

        Pragma::Kind kind = thatLine.contains("endregion") ? Pragma::EndRegion
                            : thatLine.contains("region")  ? Pragma::Region
                                                           : Pragma::Other;

        auto& directive = directives[fid];
        directive.pragmas.emplace_back(Pragma{
            thatLine,
            kind,
            Loc,
        });
    }

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
};

}  // namespace

void Directive::attach(clang::Preprocessor& pp,
                       llvm::DenseMap<clang::FileID, Directive>& directives) {
    pp.addPPCallbacks(std::make_unique<PPCallback>(pp, directives));
}

}  // namespace clice
