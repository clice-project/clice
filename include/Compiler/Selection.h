#pragma once

#include <Compiler/Clang.h>

namespace clice {

// Code Action:
// add implementation in cpp file(important).
// extract implementation to cpp file(important).
// generate virtual function declaration(full qualified?).
// generate c++20 coroutine and awaiter interface.
// expand macro(one step by step).
// invert if.

namespace {
class SelectionBuilder;
}

class SelectionTree {
    friend class SelectionBuilder;

public:
    /// An AST node is involved in the selection, either selected directly or some descendant node
    /// is selected.
    struct Node {
        Node* parent;
        clang::DynTypedNode node;
        llvm::SmallVector<const Node*> children;
    };

    SelectionTree() = default;

    SelectionTree(const SelectionTree&) = delete;
    SelectionTree& operator= (const SelectionTree&) = delete;

    SelectionTree(SelectionTree&&) = default;
    SelectionTree& operator= (SelectionTree&&) = default;

    // SelectionTree(std::uint32_t begin, std::uint32_t end, clang::ASTContext& context,
    //               clang::syntax::TokenBuffer& tokens);

    /// Check if there is any selection.
    bool hasValue() const {
        return root != nullptr;
    }

    /// Get the root node of the selection tree. Return nullptr if there is no selection.
    const clang::TranslationUnitDecl* getRoot() const {
        return root ? root->node.get<clang::TranslationUnitDecl>() : nullptr;
    }

    explicit operator bool () const {
        return hasValue();
    }

private:
    // The root node of selection tree. If there is any selection, the root is
    // clang::TranslationUnitDecl* (also the first node in `storage`).
    Node* root;

    // The AST nodes was stored in the order from root to leaf.
    // Use deque as the stable pointer storage.
    std::deque<Node> storage;
};

}  // namespace clice
