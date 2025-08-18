#pragma once

#include "clang/AST/Decl.h"
#include "clang/AST/TypeLoc.h"

namespace clice::ast {

/// is this decl a definition?
bool is_definition(const clang::Decl* decl);

/// Check whether the decl is a template. Note that for partial specializations,
/// we consider it as a template while clang does not.
bool is_templated(const clang::Decl* decl);

/// Return the decl where it is instantiated from. If could be a template decl
/// or a member of a class template. If the decl is a full specialization, return
/// itself.
const clang::NamedDecl* instantiated_from(const clang::NamedDecl* decl);

const clang::NamedDecl* normalize(const clang::NamedDecl* decl);

llvm::StringRef identifier_of(const clang::NamedDecl& D);

llvm::StringRef identifier_of(clang::QualType T);

/// Get the name of the decl.
std::string display_name_of(const clang::NamedDecl* decl);

/// To response go-to-type-definition request. Some decls actually have a type
/// for example the result of `typeof(var)` is the type of `var`. This function
/// returns the type for the decl if any.
clang::QualType type_of(const clang::NamedDecl* decl);

const clang::NamedDecl* get_decl_for_type(const clang::Type* T);

/// Get the underlying decl for a type if any.
const clang::NamedDecl* decl_of(clang::QualType type);

/// Check whether the decl is anonymous.
bool is_anonymous(const clang::NamedDecl* decl);

clang::NestedNameSpecifierLoc get_qualifier_loc(const clang::NamedDecl* decl);

auto get_template_specialization_args(const clang::NamedDecl* decl)
    -> std::optional<llvm::ArrayRef<clang::TemplateArgumentLoc>>;

std::string print_template_specialization_args(const clang::NamedDecl* decl);

std::string print_name(const clang::NamedDecl* decl);

/// Recursively strips all pointers, references, and array extents from
/// a TypeLoc. e.g., for "const int*(&)[3]", the result will be location
/// "for int".
auto unwrap_type(clang::TypeLoc type, bool unwrap_function_type = true) -> clang::TypeLoc;

auto get_only_instantiation(clang::NamedDecl* TemplatedDecl) -> clang::NamedDecl*;

auto get_only_instantiation(clang::ParmVarDecl* decl) -> clang::ParmVarDecl*;

std::string summarize_expr(const clang::Expr* E);

// Returns the template parameter pack type from an instantiated function
// template, if it exists, nullptr otherwise.
const clang::TemplateTypeParmType* getFunctionPackType(const clang::FunctionDecl* Callee);

// Returns the template parameter pack type that this parameter was expanded
// from (if in the Args... or Args&... or Args&&... form), if this is the case,
// nullptr otherwise.
const clang::TemplateTypeParmType* getUnderlyingPackType(const clang::ParmVarDecl* Param);

// Returns the parameters that are forwarded from the template parameters.
// For example, `template <typename... Args> void foo(Args... args)` will return
// the `args` parameters.
llvm::SmallVector<const clang::ParmVarDecl*>
    resolveForwardingParameters(const clang::FunctionDecl* D, unsigned MaxDepth = 10);

// Determines if any intermediate type in desugaring QualType QT is of
// substituted template parameter type. Ignore pointer or reference wrappers.
bool isSugaredTemplateParameter(clang::QualType QT);

// A simple wrapper for `clang::desugarForDiagnostic` that provides optional
// semantic.
std::optional<clang::QualType> desugar(clang::ASTContext& AST, clang::QualType QT);

// Apply a series of heuristic methods to determine whether or not a QualType QT
// is suitable for desugaring (e.g. getting the real name behind the using-alias
// name). If so, return the desugared type. Otherwise, return the unchanged
// parameter QT.
//
// This could be refined further. See
// https://github.com/clangd/clangd/issues/1298.
clang::QualType maybeDesugar(clang::ASTContext& AST, clang::QualType QT);

// Given a callee expression `Fn`, if the call is through a function pointer,
// try to find the declaration of the corresponding function pointer type,
// so that we can recover argument names from it.
// FIXME: This function is mostly duplicated in SemaCodeComplete.cpp; unify.
clang::FunctionProtoTypeLoc getPrototypeLoc(clang::Expr* Fn);

}  // namespace clice::ast
