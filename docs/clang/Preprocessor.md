本小节会详细介绍`Preprocessor`提供的一些给用户的接口

```cpp
using namespace clang;
class Callback : public clang::PPCallbacks {
public:
    Preprocessor& pp;
    SourceRange last;
    Callback(Preprocessor& pp) : pp(pp) {}
    /// Called by Preprocessor::HandleMacroExpandedIdentifier when a
    /// macro invocation is found.
    void MacroExpands(const Token& MacroNameTok,
                      const MacroDefinition& MD,
                      SourceRange Range,
                      const MacroArgs* Args) override {
        auto name = MacroNameTok.getIdentifierInfo()->getName();
        if(name.starts_with("__"))
            return;
        llvm::outs() << "MacroExpands: " << name;
        if(MD.getMacroInfo()->isFunctionLike()) {
            llvm::outs() << "(";
            int len = Args->getNumMacroArguments();
            for(auto i = 0; i < len; i++) {
                auto arg = Args->getUnexpArgument(i);
                auto len2 = Args->getArgLength(arg);
                for(auto j = 0; j < len2; j++) {
                    llvm::outs() << pp.getSpelling(*(arg + j));
                }
                if(i < len - 1)
                    llvm::outs() << ", ";
            }
            llvm::outs() << ")";
        }
        llvm::outs() << "\n";
        auto& m = pp.getSourceManager();
        auto x = m.getExpansionRange(Range);
        // auto z = m.getImmediateExpansionRange(x.getBegin());
        auto text = Lexer::getSourceText(x, m, pp.getLangOpts());
        llvm::outs() << text << "\n";
    }
    /// Hook called whenever a macro definition is seen.
    void MacroDefined(const Token& MacroNameTok, const MacroDirective* MD) override {
        auto name = MacroNameTok.getIdentifierInfo()->getName();
        if(name.starts_with("__"))
            return;
        // llvm::outs() << "MacroDefined: " << name << "\n";
    }
}
```

```cpp
// must be after BeginSourceFile
auto& preprocessor = instance->getPreprocessor();
preprocessor.addPPCallbacks(std::make_unique<Callback>(preprocessor));
```