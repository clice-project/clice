#pragma once

#include <Compiler/Clang.h>

namespace clice {

/// This class is used to resolve dependent names in the AST.
/// For dependent names, we cannot know the any information about the name until
/// the template is instantiated. This can be frustrating, you cannot get
/// completion, you cannot get go-to-definition, etc. To avoid this, we just use
/// some heuristics to simplify the dependent names as normal type/expression.
/// For example, `std::vector<T>::value_type` can be simplified as `T`.
class TemplateResolver {
public:
    TemplateResolver(clang::Sema& sema) : sema(sema) {}

    clang::QualType resolve(clang::QualType type);

    // TODO:
    // use a relative clear way to resolve `UnresolvedLookupExpr`.


    // TODO: desugar UsingType.
private:
    clang::Sema& sema;
};

}  // namespace clice
