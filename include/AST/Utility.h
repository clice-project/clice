#include "clang/AST/Decl.h"

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

/// Get the name of the decl.
std::string name_of(const clang::NamedDecl* decl);

/// To response go-to-type-definition request. Some decls actually have a type
/// for example the result of `typeof(var)` is the type of `var`. This function
/// returns the type for the decl if any.
clang::QualType type_of(const clang::NamedDecl* decl);

/// Get the underlying decl for a type if any.
const clang::NamedDecl* decl_of(clang::QualType type);

}  // namespace clice::ast
