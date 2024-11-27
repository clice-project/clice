// #include "Compiler/ParsedAST.h"
#include "Feature/SemanticTokens.h"
#include "Compiler/Semantic.h"

namespace clice::feature {

namespace {

static bool isKeyword(clang::tok::TokenKind kind,
                      llvm::StringRef text,
                      const clang::LangOptions& option) {
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
//     HighlightBuilder(con//st ParsedAST& parsedAST, llvm::StringRef filename) : AST(parsedAST),
//     filename(filename) {}
//
//     SemanticToken& addToken(proto::SemanticTokenType type, clang::SourceLocation begin,
//     std::size_t length) {
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
//     //        AST.sourceManager.getFileOffset(range.getEnd()) -
//     AST.sourceManager.getFileOffset(range.getBegin());
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
//                 addToken(proto::SemanticTokenType::Angle, loc,
//                 1).addModifier(proto::SemanticTokenModifier::Right);
//             } else if(token->kind() == clang::tok::greatergreater) {
//                 // TODO: split `>>` into two tokens
//                 addToken(proto::SemanticTokenType::Angle, loc,
//                 2).addModifier(proto::SemanticTokenModifier::Right);
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
//     HighlightCollector(const ParsedAST& AST, HighlightBuilder& builder) : AST(AST),
//     builder(builder) {}
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
//                 if(isKeyword(token.kind(), token.text(AST.sourceManager),
//                 AST.context.getLangOpts())) {
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

struct SemanticToken {
    std::uint32_t line;
    std::uint32_t column;
    std::uint32_t length;
    proto::SemanticTokenType type;
    std::uint32_t modifiers = 0;

    SemanticToken& addModifier(proto::SemanticTokenModifier modifier) {
        modifiers |= 1 << static_cast<std::uint32_t>(modifier);
        return *this;
    }
};

class HighlightBuilder : public SemanticVisitor<HighlightBuilder> {
public:
    HighlightBuilder(Compiler& compiler) : SemanticVisitor<HighlightBuilder>(compiler, true) {}

    void handleOccurrence(const clang::Decl* decl,
                          clang::SourceLocation location,
                          OccurrenceKind kind = OccurrenceKind::Source) {
        proto::SemanticTokenType type = proto::SemanticTokenType::Invalid;
        if(llvm::isa<clang::NamespaceDecl, clang::NamespaceAliasDecl>(decl)) {
            type = proto::SemanticTokenType::Namespace;
        } else if(llvm::isa<clang::TypedefNameDecl,
                            clang::TemplateTypeParmDecl,
                            clang::TemplateTemplateParmDecl>(decl)) {
            type = proto::SemanticTokenType::Type;
        } else if(llvm::isa<clang::EnumDecl>(decl)) {
            type = proto::SemanticTokenType::Enum;
        } else if(llvm::isa<clang::EnumConstantDecl>(decl)) {
            type = proto::SemanticTokenType::EnumMember;
        } else if(auto TD = llvm::dyn_cast<clang::TagDecl>(decl)) {
            type = TD->isStruct()  ? proto::SemanticTokenType::Struct
                   : TD->isUnion() ? proto::SemanticTokenType::Union
                                   : proto::SemanticTokenType::Class;
        } else if(llvm::isa<clang::FieldDecl>(decl)) {
            type = proto::SemanticTokenType::Field;
        } else if(llvm::isa<clang::ParmVarDecl>(decl)) {
            type = proto::SemanticTokenType::Parameter;
        } else if(llvm::isa<clang::VarDecl, clang::BindingDecl, clang::NonTypeTemplateParmDecl>(
                      decl)) {
            type = proto::SemanticTokenType::Variable;
        } else if(llvm::isa<clang::CXXMethodDecl>(decl)) {
            type = proto::SemanticTokenType::Method;
        } else if(llvm::isa<clang::FunctionDecl>(decl)) {
            type = proto::SemanticTokenType::Function;
        } else if(llvm::isa<clang::LabelDecl>(decl)) {
            type = proto::SemanticTokenType::Label;
        } else if(llvm::isa<clang::ConceptDecl>(decl)) {
            type = proto::SemanticTokenType::Concept;
        }

        if(type != proto::SemanticTokenType::Invalid) {
            addToken(location, type);
        }
    }

    void handleOccurrence(const clang::BuiltinType* type,
                          clang::SourceRange range,
                          OccurrenceKind kind = OccurrenceKind::Source) {
        // llvm::outs() << type->getName(clang::PrintingPolicy({})) << "\n";
        // dump(range.getBegin());
        // dump(range.getEnd());
    }

    void handleOccurrence(const clang::Attr* attr,
                          clang::SourceRange range,
                          OccurrenceKind kind = OccurrenceKind::Source) {
        dump(range.getBegin());
        auto tokens = tokBuf.expandedTokens(range);
        if(auto first = tokens.begin()) {
            auto second = first + 1;
            switch(first->kind()) {
                case clang::tok::identifier: {
                    bool isFirstTokenNamespace = second && second->kind() == clang::tok::coloncolon;
                    if(isFirstTokenNamespace) {
                        addToken(first->location(), proto::SemanticTokenType::Namespace);
                        first += 2;
                    }

                    assert(first && first->kind() == clang::tok::identifier &&
                           "Expecting an identifier token");
                    addToken(first->location(), proto::SemanticTokenType::Attribute);
                    break;
                }
                /// [[using CC: opt(1), debug]]
                case clang::tok::kw_using: {
                    assert(second && second->kind() == clang::tok::identifier);
                    addToken(second->location(), proto::SemanticTokenType::Namespace);
                    break;
                }
                default: {
                    std::terminate();
                }
            }
        }
    }

    void run() {
        TraverseAST(sema.getASTContext());
    }

    HighlightBuilder& addToken(clang::SourceLocation location, proto::SemanticTokenType type) {
        auto loc = srcMgr.getPresumedLoc(location);
        tokens.emplace_back(SemanticToken{
            .line = loc.getLine() - 1,
            .column = loc.getColumn() - 1,
            .length = clang::Lexer::MeasureTokenLength(location, srcMgr, sema.getLangOpts()),
            .type = type,
            .modifiers = 0,
        });
        return *this;
    }

    proto::SemanticTokens build() {
        /// Collect semantic from spelled tokens.
        auto mainFileID = srcMgr.getMainFileID();
        auto spelledTokens = tokBuf.spelledTokens(mainFileID);
        for(auto& token: spelledTokens) {
            proto::SemanticTokenType type = proto::SemanticTokenType::Invalid;
            llvm::outs() << clang::tok::getTokenName(token.kind()) << " "
                         << pp.getIdentifierInfo(token.text(srcMgr))->isKeyword(pp.getLangOpts())
                         << "\n";

            auto kind = token.kind();
            switch(kind) {
                case clang::tok::numeric_constant: {
                    type = proto::SemanticTokenType::Number;
                    break;
                }

                case clang::tok::char_constant:
                case clang::tok::wide_char_constant:
                case clang::tok::utf8_char_constant:
                case clang::tok::utf16_char_constant:
                case clang::tok::utf32_char_constant: {
                    type = proto::SemanticTokenType::Character;
                    break;
                }

                case clang::tok::string_literal:
                case clang::tok::wide_string_literal:
                case clang::tok::utf8_string_literal:
                case clang::tok::utf16_string_literal:
                case clang::tok::utf32_string_literal: {
                    type = proto::SemanticTokenType::String;
                    break;
                }

                case clang::tok::ampamp:        /// and
                case clang::tok::ampequal:      /// and_eq
                case clang::tok::amp:           /// bitand
                case clang::tok::pipe:          /// bitor
                case clang::tok::tilde:         /// compl
                case clang::tok::exclaim:       /// not
                case clang::tok::exclaimequal:  /// not_eq
                case clang::tok::pipepipe:      /// or
                case clang::tok::pipeequal:     /// or_eq
                case clang::tok::caret:         /// xor
                case clang::tok::caretequal: {  /// xor_eq
                    /// Clang will lex above keywords as corresponding operators. But we want to
                    /// highlight them as keywords. So check whether their text is same as the
                    /// operator spelling. If not, it indicates that they are keywords.
                    if(token.text(srcMgr) != clang::tok::getPunctuatorSpelling(kind)) {
                        type = proto::SemanticTokenType::Operator;
                    }
                }

                default: {
                    if(pp.getIdentifierInfo(token.text(srcMgr))->isKeyword(pp.getLangOpts())) {
                        type = proto::SemanticTokenType::Keyword;
                        break;
                    }
                }
            }

            if(type != proto::SemanticTokenType::Invalid) {
                addToken(token.location(), type);
            }
        }

        /// Collect semantic tokens from AST.
        TraverseAST(sema.getASTContext());

        proto::SemanticTokens result;
        std::ranges::sort(tokens, [](const SemanticToken& lhs, const SemanticToken& rhs) {
            return lhs.line < rhs.line || (lhs.line == rhs.line && lhs.column < rhs.column);
        });

        std::size_t lastLine = 0;
        std::size_t lastColumn = 0;

        for(auto& token: tokens) {
            result.data.push_back(token.line - lastLine);
            result.data.push_back(token.line == lastLine ? token.column - lastColumn
                                                         : token.column);
            result.data.push_back(token.length);
            result.data.push_back(llvm::to_underlying(token.type));
            result.data.push_back(token.modifiers);

            lastLine = token.line;
            lastColumn = token.column;
        }

        return result;
    }

    std::vector<SemanticToken> tokens;
};

}  // namespace

proto::SemanticTokens semanticTokens(Compiler& compiler, llvm::StringRef filename) {
    HighlightBuilder builder(compiler);
    return builder.build();
}

}  // namespace clice::feature
