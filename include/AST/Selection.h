#include "ParsedAST.h"
#include <clang/AST/ASTTypeTraits.h>

namespace clice {

class SelectionTree {
public:
    enum Selection {
        Unselected,
        Partial,
        Complete,
    };

    struct Node {
        Node* parent;
        llvm::SmallVector<const Node*> children;
        clang::DynTypedNode ASTNode;
    };

    const Node* commonAncestor() const;

    const Node& root() const;

    SelectionTree(clang::ASTContext& context, const clang::syntax::TokenBuffer& tokens, unsigned start, unsigned end);

private:
    std::deque<Node> m_Nodes;
    const Node* m_Root;
};

}  // namespace clice
