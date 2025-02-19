#include "AST/SymbolKind.h"
#include "Basic/SourceConverter.h"
#include "Compiler/Compilation.h"
#include "Feature/CodeCompletion.h"
#include "clang/Sema/CodeCompleteConsumer.h"

namespace clice::feature {

namespace {

struct CompletionPrefix {
    // The unqualified partial name.
    // If there is none, begin() == end() == completion position.
    llvm::StringRef name;

    // The spelled scope qualifier, such as Foo::.
    // If there is none, begin() == end() == name.begin().
    llvm::StringRef qualifier;

    static CompletionPrefix from(llvm::StringRef content, std::size_t offset) {
        assert(offset <= content.size());
        CompletionPrefix result;

        llvm::StringRef rest = content.take_front(offset);

        // Consume the unqualified name. We only handle ASCII characters.
        // isAsciiIdentifierContinue will let us match "0invalid", but we don't mind.
        while(!rest.empty() && clang::isAsciiIdentifierContinue(rest.back())) {
            rest = rest.drop_back();
        }

        result.name = content.slice(rest.size(), offset);

        // Consume qualifiers.
        while(rest.consume_back("::") && !rest.ends_with(":")) {
            // reject ::::
            while(!rest.empty() && clang::isAsciiIdentifierContinue(rest.back())) {
                rest = rest.drop_back();
            }
        }

        result.qualifier = content.slice(rest.size(), result.name.begin() - content.begin());
        return result;
    }
};

proto::CompletionItemKind kindForDecl(const clang::NamedDecl* decl) {
    auto kind = SymbolKind::from(decl);
    switch(kind.value()) {
        case SymbolKind::Keyword: return proto::CompletionItemKind::Keyword;
        case SymbolKind::Namespace: return proto::CompletionItemKind::Module;
        case SymbolKind::Class: return proto::CompletionItemKind::Class;
        case SymbolKind::Struct: return proto::CompletionItemKind::Struct;
        case SymbolKind::Union: return proto::CompletionItemKind::Struct;
        case SymbolKind::Enum: return proto::CompletionItemKind::Enum;
        case SymbolKind::Type: return proto::CompletionItemKind::TypeParameter;
        case SymbolKind::Field: return proto::CompletionItemKind::Field;
        case SymbolKind::EnumMember: return proto::CompletionItemKind::EnumMember;
        case SymbolKind::Function: return proto::CompletionItemKind::Function;
        case SymbolKind::Method: return proto::CompletionItemKind::Method;
        case SymbolKind::Variable: return proto::CompletionItemKind::Variable;
        case SymbolKind::Parameter: return proto::CompletionItemKind::Variable;
        case SymbolKind::Label: return proto::CompletionItemKind::Variable;
        case SymbolKind::Concept: return proto::CompletionItemKind::TypeParameter;
        case SymbolKind::Operator: return proto::CompletionItemKind::Operator;
        case SymbolKind::Comment:
        case SymbolKind::Number:
        case SymbolKind::Character:
        case SymbolKind::String:
        case SymbolKind::Directive:
        case SymbolKind::Header:
        case SymbolKind::Module:
        case SymbolKind::Macro:
        case SymbolKind::MacroParameter:
        case SymbolKind::Attribute:
        case SymbolKind::Paren:
        case SymbolKind::Bracket:
        case SymbolKind::Brace:
        case SymbolKind::Angle:
        case SymbolKind::Invalid: {
            return proto::CompletionItemKind::Text;
        };
    }

    llvm_unreachable("Unknown SymbolKind");
}

std::string getName(const clang::NamedDecl* decl) {
    auto name = decl->getDeclName();
    switch(name.getNameKind()) {
        case clang::DeclarationName::Identifier: {
            return name.getAsIdentifierInfo()->getName().str();
        }
        case clang::DeclarationName::CXXConstructorName:
        case clang::DeclarationName::CXXDestructorName: {
            return name.getCXXNameType().getAsString();
        }
        case clang::DeclarationName::CXXConversionFunctionName: {
            return "operator " + name.getCXXNameType().getAsString();
        }
        case clang::DeclarationName::CXXOperatorName: {
            return clang::getOperatorSpelling(name.getCXXOverloadedOperator());
        }
        case clang::DeclarationName::CXXDeductionGuideName: {
            return getName(name.getCXXDeductionGuideTemplate());
        }
        case clang::DeclarationName::CXXLiteralOperatorName: {
            return name.getCXXLiteralIdentifier()->getName().str();
        }
        case clang::DeclarationName::CXXUsingDirective: {
            std::abort();
        };
        case clang::DeclarationName::ObjCZeroArgSelector:
        case clang::DeclarationName::ObjCOneArgSelector:
        case clang::DeclarationName::ObjCMultiArgSelector: {
            std::abort();
        }
    }
}

class CodeCompletionCollector final : public clang::CodeCompleteConsumer {
public:
    CodeCompletionCollector(proto::CompletionResult& completions,
                            uint32_t line,
                            uint32_t column,
                            llvm::StringRef content) :
        clang::CodeCompleteConsumer({}), completions(completions),
        allocator(new clang::GlobalCodeCompletionAllocator()), info(allocator), line(line),
        column(column), content(content) {}

    void ProcessCodeCompleteResults(clang::Sema& sema,
                                    clang::CodeCompletionContext context,
                                    clang::CodeCompletionResult* results,
                                    unsigned count) final {
        // auto loc = sema.getPreprocessor().getCodeCompletionLoc();
        // auto offset = sema.getSourceManager().getFileOffset(loc);
        // auto prefix = CompletionPrefix::from(content, offset);
        //
        // for(auto& result: llvm::make_range(results, results + count)) {
        //    auto& item = completions.emplace_back();
        //    item.kind = proto::CompletionItemKind::Text;
        //    switch(result.Kind) {
        //        case clang::CodeCompletionResult::RK_Declaration: {
        //            item.label = getName(result.Declaration);
        //            item.kind = kindForDecl(result.Declaration);
        //            item.detail = result.Declaration->getNameAsString();
        //            break;
        //        }
        //        case clang::CodeCompletionResult::RK_Keyword: {
        //            item.label = result.Keyword;
        //            item.kind = proto::CompletionItemKind::Keyword;
        //            break;
        //        }
        //        case clang::CodeCompletionResult::RK_Macro: {
        //            item.label = result.Macro->getName();
        //            break;
        //        }
        //        case clang::CodeCompletionResult::RK_Pattern: {
        //            item.kind = proto::CompletionItemKind::Snippet;
        //            item.label = result.Pattern->getTypedText();
        //            break;
        //        }
        //    }
        //    item.textEdit.newText = item.label;
        //    item.textEdit.range = {
        //        .start = {line - 1, static_cast<uint32_t>(column - 1 - prefix.name.size())},
        //        .end = {line - 1, static_cast<uint32_t>(column + item.label.size()) - 1 },
        //    };
        //}
    }

    clang::CodeCompletionAllocator& getAllocator() final {
        return *allocator;
    }

    clang::CodeCompletionTUInfo& getCodeCompletionTUInfo() final {
        return info;
    }

private:
    uint32_t line;
    uint32_t column;
    llvm::StringRef content;
    std::shared_ptr<clang::GlobalCodeCompletionAllocator> allocator;
    clang::CodeCompletionTUInfo info;
    proto::CompletionResult& completions;
};

}  // namespace

json::Value capability(json::Value clientCapabilities) {
    return json::Object{
        // We don't set `(` etc as allCommitCharacters as they interact
        // poorly with snippet results.
        // See https://github.com/clangd/vscode-clangd/issues/357
        // Hopefully we can use them one day without this side-effect:
        //     https://github.com/microsoft/vscode/issues/42544
        {"resolveProvider",   false                               },
        // We do extra checks, e.g. that > is part of ->.
        {"triggerCharacters", {".", "<", ">", ":", "\"", "/", "*"}},
    };
}

proto::CompletionResult codeCompletion(CompilationParams& params,
                                       uint32_t line,
                                       uint32_t column,
                                       llvm::StringRef file,
                                       const config::CodeCompletionOption& option) {
    proto::CompletionResult completions;
    auto consumer = new CodeCompletionCollector(completions, line, column, params.content);

    params.srcPath = file;
    params.file = file;
    params.line = line;
    params.column = column;

    if(auto info = compile(params, consumer)) {
        for(auto& item: completions) {}
        return completions;
    } else {
        std::abort();
    }
}

}  // namespace clice::feature
