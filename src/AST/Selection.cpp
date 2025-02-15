#include <AST/Selection.h>
#include <Compiler/AST.h>

#include <clang/AST/RecursiveASTVisitor.h>

#include <stack>

namespace clice {

namespace {

struct SelectionBuilder {
    using Token = clang::syntax::Token;
    using OffsetPair = std::pair<std::uint32_t, std::uint32_t>;

    SelectionBuilder(std::uint32_t begin,
                     std::uint32_t end,
                     clang::ASTContext& context,
                     clang::syntax::TokenBuffer& buffer) : context(context), buffer(buffer) {
        assert(end >= begin && "End offset should be greater than or equal to begin offset.");

        // The location in clang AST is token-based, of course. Because the parser
        // processes tokens from the lexer. So we need to find boundary tokens at first.
        // FIXME: support other file.
        auto& src = context.getSourceManager();
        auto tokens = buffer.spelledTokens(src.getMainFileID());
        auto bound = selectionBound(tokens, {begin, end}, src);

        left = bound.first, right = bound.second;
    }

    /// Construct a selection builder from two boundary tokens. the `left` and `right` should come
    /// from `fixSelectionBound`.
    /// The constructor is used for unittest.
    SelectionBuilder(const Token* left,
                     const Token* right,
                     clang::ASTContext& context,
                     clang::syntax::TokenBuffer& buffer) :
        left(left), right(right), context(context), buffer(buffer) {}

    /// Compute 2 boundary tokens by given pair of offset as the selection range, the `end` of
    /// pair should be greater than `begin`.
    static auto selectionBound(llvm::ArrayRef<Token> tokens,
                               OffsetPair offsets,
                               const clang::SourceManager& src)
        -> std::pair<const Token*, const Token*> {
        auto [begin, end] = offsets;
        assert(end >= begin && "Can not build a selection range for a invalid OffsetPair");

        // int       xxxx = 3;
        //       ^^^^^^
        // expect to find the first token whose end location is greater than `begin`.
        auto left = std::partition_point(tokens.begin(), tokens.end(), [&](const auto& token) {
            return src.getFileOffset(token.endLocation()) <= begin;
        });

        // int xxxx        = 3;
        //      ^^^^^^
        // expect to find the last token whose start location is less than to `end`.
        auto right = std::partition_point(left, tokens.end(), [&](const auto& token) {
            return src.getFileOffset(token.location()) < end;
        });

        // right - 1: the right is the first token whose start location is greater than `end`.
        return {left, right - 1};
    }

    bool isValidOffsetRange() const {
        const auto tokens = buffer.spelledTokens(context.getSourceManager().getMainFileID());
        return left != tokens.end() && right != tokens.end();
    }

    template <typename Node>
    clang::SourceRange getSourceRange(const Node* node) {
        if constexpr(std::is_base_of_v<Node, clang::Attr>)
            return node->getRange();
        else
            return node->getSourceRange();
    }

    template <typename Node, typename Callback>
    bool hook(const Node* node, const Callback& callback) {
        if constexpr(requires { node->isImplicit(); })
            if(node->isImplicit())
                return true;

        clang::SourceRange range = getSourceRange(node);
        if(range.isInvalid())
            return true;

        // No overlap, the node is not selected.
        if(range.getEnd() < left->location() || range.getBegin() > right->endLocation())
            return true;

        // There is overlap between source range of node and selection, by default it is partial.
        auto coverage = SelectionTree::CoverageKind::Partial;

        // The source range of current node contains the boundary tokens. it' a full coverage.
        if(range.getBegin() <= left->location() && range.getEnd() >= right->location())
            coverage = SelectionTree::CoverageKind::Full;

        SelectionTree::Node selected{
            .dynNode = clang::DynTypedNode::create(*node),
            .kind = coverage,
            .parent = stack.empty() ? nullptr : stack.top(),
        };

        // Store the selected node and link it to its father node.
        storage.push_back(std::move(selected));
        if(!stack.empty())
            stack.top()->children.push_back(&storage.back());

        SelectionTree::Node& current = storage.back();

        // For a full coverage case, node's children may also full coverage the selection range. so
        // traverse them recursively until the node cover the selection range partially.
        if(coverage == SelectionTree::CoverageKind::Full) {
            stack.emplace(&storage.back());
            bool ret = callback();
            stack.pop();
            return ret;
        }

        /// For the given selection of a clang::TagDecl:
        ///     class X {/* something */};
        ///     ^^^^^^^^^^^^^^^^^^^^^^^^^^
        /// we correct the selection to full source range of class X without semi:
        ///     class X {/* something */};
        ///     ^^^^^^^^^^^^^^^^^^^^^^^^^
        if constexpr(std::derived_from<clang::Decl, Node>) {
            if(right->kind() == clang::tok::semi)
                current.kind = SelectionTree::CoverageKind::Full;
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

    /// the two boundary tokens.
    const clang::syntax::Token* left;
    const clang::syntax::Token* right;

    clang::ASTContext& context;
    clang::syntax::TokenBuffer& buffer;

    /// father nodes stack.
    std::stack<Node*> stack;
    std::deque<Node> storage;
};

struct SelectionCollector : public clang::RecursiveASTVisitor<SelectionCollector> {
    using Base = clang::RecursiveASTVisitor<SelectionCollector>;

    SelectionBuilder& builder;

    SelectionCollector(SelectionBuilder& builder) : builder(builder) {}

    bool TraverseDecl(clang::Decl* decl) {
        /// `TranslationUnitDecl` has invalid location information.
        /// So we process it separately.
        if(llvm::isa_and_nonnull<clang::TranslationUnitDecl>(decl)) {
            return Base::TraverseDecl(decl);
        }

        return builder.hook(decl, [&] { return Base::TraverseDecl(decl); });
    }

    bool TraverseStmt(clang::Stmt* stmt) {
        return builder.hook(stmt, [&] { return Base::TraverseStmt(stmt); });
    }

    bool TraverseAttr(clang::Attr* attr) {
        return builder.hook(attr, [&] { return Base::TraverseAttr(attr); });
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

        return builder.hook(&loc, [&] { return Base::TraverseTypeLoc(loc); });
    }

    bool TraverseNestedNameSpecifierLoc(clang::NestedNameSpecifierLoc NNS) {
        return builder.hook(&NNS, [&] { return Base::TraverseNestedNameSpecifierLoc(NNS); });
    }

    bool TraverseTemplateArgumentLoc(const clang::TemplateArgumentLoc& argument) {
        return builder.hook(&argument, [&] { return Base::TraverseTemplateArgumentLoc(argument); });
    }

    bool TraverseCXXBaseSpecifier(const clang::CXXBaseSpecifier& base) {
        return builder.hook(&base, [&] { return Base::TraverseCXXBaseSpecifier(base); });
    }

    bool TraverseConstructorInitializer(clang::CXXCtorInitializer* init) {
        return builder.hook(init, [&] { return Base::TraverseConstructorInitializer(init); });
    }

    /// FIXME: figure out concept in clang AST.
    bool TraverseConceptReference(clang::ConceptReference* concept_) {
        return true;
    }
};

SelectionTree SelectionBuilder::build() {
    SelectionCollector collector(*this);

    if(isValidOffsetRange())
        collector.TraverseAST(context);

    storage.shrink_to_fit();

    SelectionTree tree;
    tree.storage = std::move(storage);
    tree.root = &tree.storage.front();
    return tree;
}

void dumpImpl(llvm::raw_ostream& os, const SelectionTree::Node* node, clang::ASTContext& context) {
    if(node) {
        node->dynNode.dump(os, context);
        for(auto child: node->children)
            dumpImpl(os, child, context);
    }
}

}  // namespace

SelectionTree::SelectionTree(std::uint32_t begin,
                             std::uint32_t end,
                             clang::ASTContext& context,
                             clang::syntax::TokenBuffer& tokens) {
    SelectionBuilder builder(begin, end, context, tokens);
    *this = builder.build();
}

void SelectionTree::dump(llvm::raw_ostream& os, clang::ASTContext& context) const {
    if(hasValue())
        dumpImpl(os, root, context);
}

}  // namespace clice
