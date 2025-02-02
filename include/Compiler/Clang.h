#pragma once

#include "Support/Support.h"

#include "clang/Lex/Preprocessor.h"
#include "clang/AST/ASTTypeTraits.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Syntax/Tokens.h"
#include "clang/Sema/Sema.h"
#include "AST/SourceLocation.h"

namespace clice {

#define VISIT_DECL(type) bool Visit##type(const clang::type* decl)
#define VISIT_STMT(type) bool Visit##type(const clang::type* stmt)
#define VISIT_EXPR(type) bool Visit##type(const clang::type* expr)
#define VISIT_TYPE(type) bool Visit##type(const clang::type* type)
#define VISIT_TYPELOC(type) bool Visit##type(clang::type loc)

#define TRAVERSE_DECL(type) bool Traverse##type(clang::type* decl)

using lookup_result = clang::DeclContext::lookup_result;

}  // namespace clice
