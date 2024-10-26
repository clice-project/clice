// #include "Compiler/ParsedAST.h"
#include "Feature/SemanticTokens.h"

namespace clice::feature {

namespace {

struct SemanticToken {
    clang::SourceLocation begin;
    std::size_t length;
    proto::SemanticTokenType type;
    std::uint32_t modifiers = 0;

    SemanticToken& addModifier(proto::SemanticTokenModifier modifier) {
        modifiers |= 1 << static_cast<std::uint32_t>(modifier);
        return *this;
    }
};

static bool isKeyword(clang::tok::TokenKind kind, llvm::StringRef text, const clang::LangOptions& option) {
    switch(kind) {
        case clang::tok::kw_void:
        case clang::tok::kw_int:
        case clang::tok::kw_char:
        case clang::tok::kw_long:
        case clang::tok::kw_short:
        case clang::tok::kw_signed:
        case clang::tok::kw_unsigned:
        case clang::tok::kw_float:
        case clang::tok::kw_double:
        case clang::tok::kw_const:
        case clang::tok::kw_volatile:
        case clang::tok::kw_auto:
        case clang::tok::kw_static:
        case clang::tok::kw_register:
        case clang::tok::kw_extern:
        case clang::tok::kw_if:
        case clang::tok::kw_else:
        case clang::tok::kw_switch:
        case clang::tok::kw_case:
        case clang::tok::kw_default:
        case clang::tok::kw_do:
        case clang::tok::kw_for:
        case clang::tok::kw_while:
        case clang::tok::kw_break:
        case clang::tok::kw_continue:
        case clang::tok::kw_goto:
        case clang::tok::kw_return:
        case clang::tok::kw_struct:
        case clang::tok::kw_union:
        case clang::tok::kw_enum:
        case clang::tok::kw_typedef:
        case clang::tok::kw_sizeof: {
            return true;
        }

        case clang::tok::amp: return option.CPlusPlus && text == "bitand";
        case clang::tok::ampamp: return option.CPlusPlus && text == "and";
        case clang::tok::ampequal: return option.CPlusPlus && text == "and_eq";
        case clang::tok::pipe: return option.CPlusPlus && text == "bitor";
        case clang::tok::pipepipe: return option.CPlusPlus && text == "or";
        case clang::tok::pipeequal: return option.CPlusPlus && text == "or_eq";
        case clang::tok::caret: return option.CPlusPlus && text == "xor";
        case clang::tok::caretequal: return option.CPlusPlus && text == "xor_eq";
        case clang::tok::exclaim: return option.CPlusPlus && text == "not";
        case clang::tok::exclaimequal: return option.CPlusPlus && text == "not_eq";
        case clang::tok::tilde: return option.CPlusPlus && text == "compl";

        case clang::tok::kw_asm:
        case clang::tok::kw_wchar_t:
        case clang::tok::kw_try:
        case clang::tok::kw_throw:
        case clang::tok::kw_catch:
        case clang::tok::kw_typeid:
        case clang::tok::kw_this:
        case clang::tok::kw_friend:
        case clang::tok::kw_mutable:
        case clang::tok::kw_explicit:
        case clang::tok::kw_virtual:
        case clang::tok::kw_operator:
        case clang::tok::kw_class:
        case clang::tok::kw_public:
        case clang::tok::kw_protected:
        case clang::tok::kw_private:
        case clang::tok::kw_using:
        case clang::tok::kw_namespace:
        case clang::tok::kw_template:
        case clang::tok::kw_typename:
        case clang::tok::kw_export:
        case clang::tok::kw_const_cast:
        case clang::tok::kw_static_cast:
        case clang::tok::kw_dynamic_cast:
        case clang::tok::kw_reinterpret_cast:
        case clang::tok::kw_new:
        case clang::tok::kw_delete: {
            return option.CPlusPlus;
        }

        case clang::tok::kw_restrict:
        case clang::tok::kw__Bool:
        case clang::tok::kw__Complex:
        case clang::tok::kw__Imaginary: {
            return option.C99;
        }

        case clang::tok::kw_inline: {
            return option.CPlusPlus || option.C99;
        }

        case clang::tok::kw__Alignas:
        case clang::tok::kw__Alignof:
        case clang::tok::kw__Atomic:
        case clang::tok::kw__Generic:
        case clang::tok::kw__Noreturn:
        case clang::tok::kw__Static_assert:
        case clang::tok::kw__Thread_local: {
            return option.C11;
        }

        case clang::tok::kw_typeof:
        case clang::tok::kw_typeof_unqual:
        case clang::tok::kw__BitInt:
        case clang::tok::kw__Decimal32:
        case clang::tok::kw__Decimal64:
        case clang::tok::kw__Decimal128: {
            return option.C23;
        }

        case clang::tok::kw_bool:
        case clang::tok::kw_true:
        case clang::tok::kw_false: {
            return option.CPlusPlus || option.C23;
        }

        case clang::tok::kw_char16_t:
        case clang::tok::kw_char32_t:
        case clang::tok::kw_noexcept:
        case clang::tok::kw_decltype: {
            return option.CPlusPlus11;
        }

        case clang::tok::kw_nullptr:
        case clang::tok::kw_alignas:
        case clang::tok::kw_alignof:
        case clang::tok::kw_thread_local:
        case clang::tok::kw_static_assert:
        case clang::tok::kw_constexpr: {
            return option.CPlusPlus11 || option.C23;
        }

        case clang::tok::kw_char8_t:
        case clang::tok::kw_import:
        case clang::tok::kw_module:
        case clang::tok::kw_constinit:
        case clang::tok::kw_consteval:
        case clang::tok::kw_concept:
        case clang::tok::kw_requires:
        case clang::tok::kw_co_await:
        case clang::tok::kw_co_yield:
        case clang::tok::kw_co_return: {
            return option.CPlusPlus20;
        }

        default: break;
    }
    return false;
}

// class HighlightBuilder {//
// public://
//     HighlightBuilder(con//st ParsedAST& parsedAST, llvm::StringRef filename) : AST(parsedAST), filename(filename) {}
//
//     SemanticToken& addToken(proto::SemanticTokenType type, clang::SourceLocation begin, std::size_t length) {
//         result.emplace_back(begin, length, type, 0);
//         return result.back();
//     }
//
//     SemanticToken& addToken(proto::SemanticTokenType type, clang::SourceLocation loc) {
//         auto token = AST.tokenBuffer.spelledTokenContaining(loc);
//         if(token) {
//             return addToken(type, token->location(), token->length());
//         }
//     }
//
//     // FIXME: source range can be a multi-line range, split it into multiple tokens
//     // SemanticToken& addToken(proto::SemanticTokenType type, clang::SourceRange range) {
//     //    SemanticToken token;
//     //    token.begin = range.getBegin();
//     //    token.length =
//     //        AST.sourceManager.getFileOffset(range.getEnd()) - AST.sourceManager.getFileOffset(range.getBegin());
//     //    token.type = type;
//     //    result.emplace_back(std::move(token));
//     //    return result.back();
//     //}
//
//     void addAngle(clang::SourceLocation left, clang::SourceLocation right) {
//         if(left.isInvalid() || right.isInvalid()) {
//             return;
//         }
//
//         llvm::outs() << "is macro?: " << left.isMacroID() << "               ";
//         left.dump(AST.sourceManager);
//         llvm::outs() << "is macro?: " << right.isMacroID() << "               ";
//         right.dump(AST.sourceManager);
//
//         if(auto token = AST.tokenBuffer.spelledTokenContaining(left)) {
//             addToken(proto::SemanticTokenType::Angle, token->location(), token->length())
//                 .addModifier(proto::SemanticTokenModifier::Left);
//         }
//
//         // RLoc might be pointing at a virtual buffer when it's part of a `>>` token.
//         auto loc = AST.sourceManager.getFileLoc(right);
//         if(auto token = AST.tokenBuffer.spelledTokenContaining(loc)) {
//             if(token->kind() == clang::tok::greater) {
//                 addToken(proto::SemanticTokenType::Angle, loc, 1).addModifier(proto::SemanticTokenModifier::Right);
//             } else if(token->kind() == clang::tok::greatergreater) {
//                 // TODO: split `>>` into two tokens
//                 addToken(proto::SemanticTokenType::Angle, loc, 2).addModifier(proto::SemanticTokenModifier::Right);
//             }
//         }
//     }
//
//     std::vector<SemanticToken> build();
//
// private:
//     const ParsedAST& AST;
//     llvm::StringRef filename;
//     std::vector<SemanticToken> result;
// };
//
///// Collect highlight information from AST.
// class HighlightCollector : public clang::RecursiveASTVisitor<HighlightCollector> {
// public:
//     HighlightCollector(const ParsedAST& AST, HighlightBuilder& builder) : AST(AST), builder(builder) {}
//
//     // Traverse(TranslationUnitDecl) {
//     //     for(auto decl: node->decls()) {
//     //         // FIXME: some decls are located in their parents' file
//     //         // e.g. `ClassTemplateSpecializationDecl`, find and exclude them
//     //         node->getLexicalDeclContext();
//     //         // if(sourceManager.isInFileID(decl->getLocation(), fileID)) {
//     //         //     TraverseDecl(decl);
//     //         // }
//     //     }
//     //     return true;
//     // }
//
// #define Traverse(NAME) bool Traverse##NAME(clang::NAME* node)
// #define WalkUpFrom(NAME) bool WalkUpFrom##NAME(clang::NAME* node)
// #define VISIT(NAME) bool Visit##NAME(clang::NAME* node)
// #define VISIT_TYPE(NAME) bool Visit##NAME(clang::NAME node)
//
//     // WalkUpFrom(NamespaceDecl) {}
//
//     VISIT(ImportDecl) {
//         return true;
//     }
//
//     VISIT(NamedDecl) {
//         return true;
//     }
//
//     VISIT(NamespaceDecl) {
//         builder.addToken(proto::SemanticTokenType::Namespace, node->getLocation());
//         return true;
//     }
//
//     VISIT(VarDecl) {
//         builder.addToken(proto::SemanticTokenType::Variable, node->getLocation());
//         return true;
//     }
//
//     VISIT(DeclaratorDecl) {
//         for(unsigned i = 0; i < node->getNumTemplateParameterLists(); ++i) {
//             if(auto params = node->getTemplateParameterList(i)) {
//                 builder.addAngle(params->getLAngleLoc(), params->getRAngleLoc());
//             }
//         }
//         return true;
//     }
//
//     VISIT(TagDecl) {
//         for(unsigned i = 0; i < node->getNumTemplateParameterLists(); ++i) {
//             if(auto params = node->getTemplateParameterList(i)) {
//                 builder.addAngle(params->getLAngleLoc(), params->getRAngleLoc());
//             }
//         }
//         return true;
//     }
//
//     VISIT(FunctionDecl) {
//         if(auto args = node->getTemplateSpecializationArgsAsWritten()) {
//             builder.addAngle(args->getLAngleLoc(), args->getRAngleLoc());
//         }
//         builder.addToken(proto::SemanticTokenType::Function, node->getLocation());
//         return true;
//     }
//
//     VISIT(TemplateDecl) {
//         if(auto params = node->getTemplateParameters()) {
//             builder.addAngle(params->getLAngleLoc(), params->getRAngleLoc());
//         }
//         return true;
//     }
//
//     VISIT(ClassTemplateSpecializationDecl) {
//         if(auto args = node->getTemplateArgsAsWritten()) {
//             builder.addAngle(args->getLAngleLoc(), args->getRAngleLoc());
//         }
//         return true;
//     }
//
//     VISIT(ClassTemplatePartialSpecializationDecl) {
//         if(auto params = node->getTemplateParameters()) {
//             builder.addAngle(params->getLAngleLoc(), params->getRAngleLoc());
//         }
//         return true;
//     }
//
//     VISIT(VarTemplateSpecializationDecl) {
//         if(auto args = node->getTemplateArgsAsWritten()) {
//             builder.addAngle(args->LAngleLoc, args->RAngleLoc);
//         }
//         return true;
//     }
//
//     VISIT(VarTemplatePartialSpecializationDecl) {
//         if(auto params = node->getTemplateParameters()) {
//             builder.addAngle(params->getLAngleLoc(), params->getRAngleLoc());
//         }
//         return true;
//     }
//
//     VISIT(OverloadExpr) {
//         builder.addAngle(node->getLAngleLoc(), node->getRAngleLoc());
//         return true;
//     }
//
//     VISIT(DeclRefExpr) {
//         builder.addToken(proto::SemanticTokenType::Variable, node->getLocation());
//         node->getEnumConstantDecl();
//         builder.addAngle(node->getLAngleLoc(), node->getRAngleLoc());
//         return true;
//     }
//
//     VISIT(CXXNamedCastExpr) {
//         builder.addAngle(node->getAngleBrackets().getBegin(), node->getAngleBrackets().getEnd());
//         return true;
//     }
//
//     VISIT(DependentScopeDeclRefExpr) {
//         // `T::value<...>`
//         //       ^  ^^^^^~~~ Angles
//         //       ^~~~ DependentValue
//         builder.addAngle(node->getLAngleLoc(), node->getRAngleLoc());
//         builder.addToken(proto::SemanticTokenType::Variable, node->getLocation())
//             .addModifier(proto::SemanticTokenModifier::Dependent);
//         return true;
//     }
//
//     VISIT(CXXDependentScopeMemberExpr) {
//         builder.addAngle(node->getLAngleLoc(), node->getRAngleLoc());
//         return true;
//     }
//
//     VISIT_TYPE(RecordTypeLoc) {
//         // `struct X x;`
//         //         ^ Type
//         builder.addToken(proto::SemanticTokenType::Type, node.getNameLoc());
//         return true;
//     }
//
//     VISIT_TYPE(DependentNameTypeLoc) {
//         // `typename T::type`
//         //               ^~~~ DependentType
//         builder.addToken(proto::SemanticTokenType::Type, node.getNameLoc())
//             .addModifier(proto::SemanticTokenModifier::Dependent);
//         return true;
//     }
//
//     VISIT_TYPE(TemplateTypeParmTypeLoc) {
//         // `typename T::type`
//         //           ^~~~ Type
//         builder.addToken(proto::SemanticTokenType::Type, node.getNameLoc());
//         return true;
//     }
//
//     VISIT_TYPE(TemplateSpecializationTypeLoc) {
//         // `Template<...>`
//         //     ^    ^~~~ Angles
//         //     ^~~~ Type
//         builder.addAngle(node.getLAngleLoc(), node.getRAngleLoc());
//         builder.addToken(proto::SemanticTokenType::Type, node.getTemplateNameLoc())
//             .addModifier(proto::SemanticTokenModifier::Templated);
//         return true;
//     }
//
//     VISIT_TYPE(DependentTemplateSpecializationTypeLoc) {
//         builder.addAngle(node.getLAngleLoc(), node.getRAngleLoc());
//         return true;
//     }
//
//     bool TraverseNestedNameSpecifierLoc(clang::NestedNameSpecifierLoc loc) {
//         if(clang::NestedNameSpecifier* NNS = loc.getNestedNameSpecifier()) {
//             if(NNS->getKind() == clang::NestedNameSpecifier::Identifier) {
//                 // `T::type::`
//                 //      ^~~~ DependentType
//                 builder.addToken(proto::SemanticTokenType::Type, loc.getLocalBeginLoc())
//                     .addModifier(proto::SemanticTokenModifier::Dependent);
//             }
//         }
//         return RecursiveASTVisitor::TraverseNestedNameSpecifierLoc(loc);
//     }
//
// private:
//     const ParsedAST& AST;
//     HighlightBuilder& builder;
// };
//
// std::vector<SemanticToken> HighlightBuilder::build() {
//     auto fileID = AST.getFileID(filename);
//     auto tokens = AST.spelledTokens(fileID);
//
//     // highlight from tokens.
//     for(auto& token: tokens) {
//         proto::SemanticTokenType type = proto::SemanticTokenType::LAST_TYPE;
//
//         switch(token.kind()) {
//             case clang::tok::TokenKind::numeric_constant: {
//                 type = proto::SemanticTokenType::Number;
//                 break;
//             }
//             case clang::tok::char_constant:
//             case clang::tok::wide_char_constant:
//             case clang::tok::utf8_char_constant:
//             case clang::tok::utf16_char_constant:
//             case clang::tok::utf32_char_constant: {
//                 type = proto::SemanticTokenType::Character;
//                 break;
//             }
//             case clang::tok::string_literal:
//             case clang::tok::wide_string_literal:
//             case clang::tok::utf8_string_literal:
//             case clang::tok::utf16_string_literal:
//             case clang::tok::utf32_string_literal: {
//                 type = proto::SemanticTokenType::String;
//                 break;
//             }
//             default: {
//                 if(isKeyword(token.kind(), token.text(AST.sourceManager), AST.context.getLangOpts())) {
//                     type = proto::SemanticTokenType::Keyword;
//                     break;
//                 }
//             }
//         }
//
//         if(type != proto::SemanticTokenType::LAST_TYPE) {
//             addToken(type, token.location(), token.length());
//         }
//     }
//
//     // TODO: highlight from preprocessor.
//
//     // highlight from AST.
//     HighlightCollector collector(AST, *this);
//     collector.TraverseTranslationUnitDecl(AST.context.getTranslationUnitDecl());
//     // AST.context.getTranslationUnitDecl()->dump();
//
//     llvm::sort(result, [](const SemanticToken& lhs, const SemanticToken& rhs) {
//         return lhs.begin < rhs.begin;
//     });
//
//     return std::move(result);
// }
//
// }  // namespace
//
// proto::SemanticTokens semanticTokens(const ParsedAST& AST, llvm::StringRef filename) {
//     HighlightBuilder builder(AST, filename);
//     std::vector<SemanticToken> tokens = builder.build();
//
//     proto::SemanticTokens result;
//     unsigned int last_line = 0;
//     unsigned int last_column = 0;
//     /// FXIME: resolve position encoding
//     for(auto& token: tokens) {
//         auto line = AST.sourceManager.getPresumedLineNumber(token.begin) - 1;
//         auto column = AST.sourceManager.getPresumedColumnNumber(token.begin) - 1;
//         result.data.push_back(line - last_line);
//         result.data.push_back(line == last_line ? column - last_column : column);
//         result.data.push_back(token.length);
//         result.data.push_back(token.type);
//         result.data.push_back(token.modifiers);
//
//         last_line = line;
//         last_column = column;
//     }
//
//     return result;
// };
//
}  // namespace

}  // namespace clice::feature
