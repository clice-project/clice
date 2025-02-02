#pragma once

#include <deque>
#include "clang/AST/ASTTypeTraits.h"
#include "clang/Tooling/Syntax/Tokens.h"

namespace clice {

// Code Action:
// add implementation in cpp file(important).
// extract implementation to cpp file(important).
// generate virtual function declaration(full qualified?).
// generate c++20 coroutine and awaiter interface.
// expand macro(one step by step).
// invert if.

class SelectionTree {
public:
    struct Node {
        Node* parent;
        clang::DynTypedNode node;
        llvm::SmallVector<const Node*> children;
    };

    SelectionTree() = default;

    SelectionTree(std::uint32_t begin,
                  std::uint32_t end,
                  clang::ASTContext& context,
                  clang::syntax::TokenBuffer& tokens);

    Node* root;
    std::deque<Node> storage;
};

}  // namespace clice
