#include "Compiler/Module.h"
#include "Compiler/Compiler.h"

namespace clice {

std::string scanModuleName(CompilationParams& params) {
    /// Because [P3034](https://github.com/cplusplus/papers/issues/1696) has been
    /// accepted, the module name in module declaration cannot be a macro now.
    /// It means that if the module declaration doesn't occur in condition preprocess
    /// directive, we can determine the module name just by lexing the source file.
    clang::LangOptions langOpts;
    langOpts.Modules = true;
    langOpts.CPlusPlus20 = true;

    /// We use raw mode of lexer to avoid the preprocessor.
    clang::Lexer lexer(clang::SourceLocation(),
                       langOpts,
                       params.content.begin(),
                       params.content.begin(),
                       params.content.end());

    /// Whether we are in a condition directive.
    bool isInDirective = false;

    /// Whether we need to preprocess the source file.
    bool needPreprocess = false;

    std::string name;
    clang::Token token;
    while(!lexer.LexFromRawLexer(token)) {
        auto kind = token.getKind();
        if(kind == clang::tok::hash) {
            lexer.LexFromRawLexer(token);
            auto diretive = token.getRawIdentifier();
            if(diretive == "if" || diretive == "ifdef" || diretive == "ifndef") {
                isInDirective = true;
            } else if(diretive == "endif") {
                isInDirective = false;
            }
        } else if(kind == clang::tok::raw_identifier) {
            if(token.getRawIdentifier() != "export") [[likely]] {
                continue;
            }

            lexer.LexFromRawLexer(token);
            if(token.getRawIdentifier() != "module") {
                continue;
            }

            /// We are after `export module`.
            if(isInDirective) {
                /// If the module name occurs in a condition directive, we have
                /// to preprocess the source file to determine the module name.
                needPreprocess = true;
                break;
            }

            /// Otherwise, we can determine the module name directly.
            while(!lexer.LexFromRawLexer(token)) {
                kind = token.getKind();
                if(kind == clang::tok::raw_identifier) {
                    name += token.getRawIdentifier();
                } else if(kind == clang::tok::colon) {
                    name += ":";
                } else if(kind == clang::tok::period) {
                    name += ".";
                } else {
                    break;
                }
            }
            return name;
        } else {
            continue;
        }
    }

    /// If not need to preprocess and the function doesn't return, it means
    /// that this file is not a module interface unit.
    if(!needPreprocess) {
        return "";
    }

    auto info = scanModule(params);
    if(!info) {
        return "";
    }

    return info->name;
}

}  // namespace clice
