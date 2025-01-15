#pragma once

#include "Support/Support.h"

#include "clang/Lex/Preprocessor.h"
#include "clang/AST/ASTTypeTraits.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Syntax/Tokens.h"
#include "clang/Sema/Sema.h"

namespace std {

template <>
struct tuple_size<clang::SourceRange> : std::integral_constant<std::size_t, 2> {};

template <>
struct tuple_element<0, clang::SourceRange> {
    using type = clang::SourceLocation;
};

template <>
struct tuple_element<1, clang::SourceRange> {
    using type = clang::SourceLocation;
};

}  // namespace std

namespace clang {

template <std::size_t I>
clang::SourceLocation get(clang::SourceRange range) {
    if constexpr(I == 0) {
        return range.getBegin();
    } else {
        return range.getEnd();
    }
}

#define VISIT_DECL(type) bool Visit##type(const clang::type* decl)
#define VISIT_STMT(type) bool Visit##type(const clang::type* stmt)
#define VISIT_EXPR(type) bool Visit##type(const clang::type* expr)
#define VISIT_TYPE(type) bool Visit##type(const clang::type* type)
#define VISIT_TYPELOC(type) bool Visit##type(clang::type loc)

#define TRAVERSE_DECL(type) bool Traverse##type(clang::type* decl)

using lookup_result = clang::DeclContext::lookup_result;

}  // namespace clang
