

#include <stack>
#include <exception>

#include <Compiler/Selection.h>

namespace clice {

namespace {

class SelectionBuilder {
public:
    SelectionBuilder(std::uint32_t begin,
                     std::uint32_t end,
                     clang::ASTContext& context,
                     clang::syntax::TokenBuffer& buffer) : context(context), buffer(buffer) {
        // The location in clang AST is token-based, of course. Because the parser
        // processes tokens from the lexer. So we need to find boundary tokens at first.
        auto& sm = context.getSourceManager();  // FIXME: support other file.
        auto tokens = buffer.spelledTokens(sm.getMainFileID());

        left = std::to_address(std::partition_point(tokens.begin(), tokens.end(), [&](const auto& token) {
            // int       xxxx = 3;
            //       ^^^^^^
            // expect to find the first token whose end location is greater than or equal to `begin`.
            return sm.getFileOffset(token.endLocation()) < begin;
        }));

        rigth = std::to_address(std::partition_point(tokens.rbegin(), tokens.rend(), [&](const auto& token) {
            // int xxxx        = 3;
            //      ^^^^^^
            // expect to find the first token whose start location is less than or equal to `end`.
            return sm.getFileOffset(token.location()) > end;
        }));

        if(left == tokens.end() || rigth == tokens.end()) {
            std::terminate();
            return;
        }
    }

    template <typename Node>
    auto getSourceRange(const Node* node) -> clang::SourceRange {
        if constexpr(std::is_base_of_v<Node, clang::Attr>) {
            return node->getRange();
        } else {
            return node->getSourceRange();
        }
    }

    template <typename Node>
    bool isSkippable(const Node* node) {
        if constexpr(requires { node->isImplicit(); }) {
            if(node->isImplicit()) {
                return true;
            }
        }

        auto range = getSourceRange(node);
        if(range.isInvalid()) {
            return true;
        }

        // range.dump(context.getSourceManager());
        // dump(node);

        return false;
    }

    template <typename Node, typename Callback>
    bool hook(const Node* node, const Callback& callback) {
        if(isSkippable(node)) {
            return true;
        }

        storage.emplace_back(SelectionTree::Node{nullptr, clang::DynTypedNode::create(*node)});
        auto range = getSourceRange(node);

        llvm::outs() << "-----------------------------------------\n";
        range.dump(context.getSourceManager());
        clang::SourceRange(left->location(), rigth->location()).dump(context.getSourceManager());

        // FIXME: currently we only consider fully nested case.
        // consider supporting partially nested case.

        // if the boundary tokens contain the source range of node, it means
        // the node is selected. store the father node and skip its children.
        if(left->location() <= range.getBegin() && rigth->location() >= range.getEnd()) {
            if(!stack.empty()) {
                llvm::outs() << "selected\n";
                stack.top()->children.push_back(&storage.back());
            }
            return true;
        }

        if(range.getBegin() <= left->location() && range.getEnd() >= rigth->location()) {
            // if the source range of node contains the boundary tokens, its
            // children may be selected. so traverse them recursively.
            llvm::outs() << "select\n";
            stack.emplace(&storage.back());
            bool ret = callback();
            return ret;
        }

        return true;
    }

    template <typename Node>
    void dump(const Node* node) {
        if constexpr(requires { node->dump(); }) {
            node->dump();
        }

        if constexpr(std::is_same_v<Node, clang::NestedNameSpecifierLoc>) {
            const clang::NestedNameSpecifierLoc& NNSL = *node;
            NNSL.getNestedNameSpecifier()->dump();
            llvm::outs() << "\n";
        }

        if constexpr(std::is_same_v<Node, clang::Attr>) {
            const clang::Attr& attr = *node;
            attr.getScopeLoc().dump(context.getSourceManager());
            attr.printPretty(llvm::outs(), context.getPrintingPolicy());
            llvm::outs() << "\n";
        }
    }

    using Node = SelectionTree::Node;

    SelectionTree build();

private:
    /// the two boundary tokens.
    const clang::syntax::Token* left;
    const clang::syntax::Token* rigth;
    clang::ASTContext& context;
    clang::syntax::TokenBuffer& buffer;
    /// father nodes stack.
    std::stack<Node*> stack;
    std::deque<Node> storage;
};

/*/

/*/
class SelectionCollector : public clang::RecursiveASTVisitor<SelectionCollector> {
public:
    SelectionCollector(SelectionBuilder& builder) : builder(builder) {}

    using Base = clang::RecursiveASTVisitor<SelectionCollector>;

    bool TraverseDecl(clang::Decl* decl) {
        /// `TranslationUnitDecl` has invalid location information.
        /// So we process it separately.
        if(llvm::isa_and_nonnull<clang::TranslationUnitDecl>(decl)) {
            return Base::TraverseDecl(decl);
        }

        return builder.hook(decl, [&] {
            return Base::TraverseDecl(decl);
        });
    }

    bool TraverseStmt(clang::Stmt* stmt) {
        return builder.hook(stmt, [&] {
            return Base::TraverseStmt(stmt);
        });
    }

    bool TraverseAttr(clang::Attr* attr) {
        return builder.hook(attr, [&] {
            return Base::TraverseAttr(attr);
        });
    }

    /// we don't care about the node without location information, so skip them.
    bool shouldWalkTypesOfTypeLocs() {
        return false;
    }

    bool TraverseType(clang::QualType) {
        return true;
    }

    bool TraverseNestedNameSpecifier(clang::NestedNameSpecifier*) {
        return true;
    }

    bool TraverseTypeLoc(clang::TypeLoc loc) {
        /// clang currently doesn't record any information for `QualifiedTypeLoc`.
        /// It has same location with its inner type. So we just ignore it.
        if(auto QTL = loc.getAs<clang::QualifiedTypeLoc>()) {
            return TraverseTypeLoc(QTL.getUnqualifiedLoc());
        }

        return builder.hook(&loc, [&] {
            return Base::TraverseTypeLoc(loc);
        });
    }

    bool TraverseNestedNameSpecifierLoc(clang::NestedNameSpecifierLoc NNS) {
        return builder.hook(&NNS, [&] {
            return Base::TraverseNestedNameSpecifierLoc(NNS);
        });
    }

    bool TraverseTemplateArgumentLoc(const clang::TemplateArgumentLoc& argument) {
        return builder.hook(&argument, [&] {
            return Base::TraverseTemplateArgumentLoc(argument);
        });
    }

    bool TraverseCXXBaseSpecifier(const clang::CXXBaseSpecifier& base) {
        return builder.hook(&base, [&] {
            return Base::TraverseCXXBaseSpecifier(base);
        });
    }

    bool TraverseConstructorInitializer(clang::CXXCtorInitializer* init) {
        return builder.hook(init, [&] {
            return Base::TraverseConstructorInitializer(init);
        });
    }

    // bool TraverseDeclarationNameInfo(clang::DeclarationNameInfo info) {
    //     return builder.hook(&info, [&] {
    //         return Base::TraverseDeclarationNameInfo(info);
    //     });
    // }

    // FIXME: figure out concept in clang AST.
    bool TraverseConceptReference(clang::ConceptReference* concept_) {
        return true;
    }

private:
    SelectionBuilder& builder;
};

SelectionTree SelectionBuilder::build() {
    SelectionCollector collector(*this);
    collector.TraverseAST(context);

    SelectionTree tree;
    tree.root = stack.empty() ? nullptr : stack.top();
    tree.storage = std::move(storage);
    return tree;
}

void dump(const SelectionTree::Node* node, clang::ASTContext& context) {
    if(node) {
        node->node.dump(llvm::outs(), context);
        for(auto child: node->children) {
            dump(child, context);
        }
    }
}

}  // namespace

SelectionTree::SelectionTree(std::uint32_t begin,
                             std::uint32_t end,
                             clang::ASTContext& context,
                             clang::syntax::TokenBuffer& tokens) {

    SelectionBuilder builder(begin, end, context, tokens);
    auto tree = builder.build();
    root = tree.root;
    llvm::outs() << "----------------------------------------\n";
    dump(root, context);
}

}  // namespace clice
