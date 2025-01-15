#include "Clang.h"

namespace clice {

/// is this decl a definition?
bool isDefinition(const clang::Decl* decl);

/// Return the decl where it is instantiated from. If could be a template decl
/// or a member of a class template. If the decl is a full specialization, return
/// itself.
const clang::NamedDecl* instantiatedFrom(const clang::NamedDecl* decl);

/// To response go-to-type-definition request. Some decls actually have a type
/// for example the result of `typeof(var)` is the type of `var`. This function
/// returns the type for the decl if any.
clang::QualType typeForDecl(const clang::NamedDecl* decl);

/// Get the underlying decl for a type if any.
const clang::NamedDecl* declForType(clang::QualType type);

void dumpInclude(clang::SourceManager& srcMgr, clang::SourceLocation loc);

}  // namespace clice
