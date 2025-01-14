#include "src/Compiler/Selection.cpp"
#include "Basic/SourceConverter.h"
#include <gtest/gtest.h>

#include "../Test.h"

namespace clice {

namespace {

using OffsetRange = std::pair<std::uint32_t, std::uint32_t>;

OffsetRange takeWholeFile(ASTInfo& info) {
    auto& src = info.srcMgr();
    auto fileID = src.getMainFileID();
    auto begin = src.getFileOffset(src.getLocForStartOfFile(fileID));
    auto end = src.getFileOffset(src.getLocForEndOfFile(fileID));
    return {begin, end};
}

void debug(llvm::raw_ostream& os, const SelectionTree::Node* node, bool showCoverage = true,
           size_t depth = 0) {
    for(auto i = 0; i < depth; i++)
        os << " ";

    if(auto typeLoc = node->dynNode.get<clang::TypeLoc>()) {
        if(typeLoc->getTypeLocClass() == clang::TypeLoc::TypeLocClass::Qualified)
            os << "QualifiedTypeLoc";
        else
            os << typeLoc->getType()->getTypeClassName() << "TypeLoc";
    } else
        os << node->dynNode.getNodeKind().asStringRef();

    if(showCoverage)
        os << '(' << refl::enum_name(node->kind) << ')';

    os << '\n';

    for(auto& child: node->children)
        debug(os, child, showCoverage, depth + 1);
}

void debug(const SelectionTree& tree) {
    if(tree) {
        llvm::outs() << "----------------------------------------\n";
        debug(llvm::outs(), tree.getRoot());
    }
}

struct SelectionTester : public Tester {

    const SourceConverter cvtr = SourceConverter(proto::PositionEncodingKind::UTF8);

    SelectionTester(llvm::StringRef file, llvm::StringRef content) : Tester(file, content) {}

    std::uint32_t getOffsetAt(llvm::StringRef id) {
        return cvtr.toOffset(params.content, locations.at(id));
    }

    void expectPreorderSequence(const SelectionTree& tree,
                                llvm::ArrayRef<clang::ASTNodeKind> kinds) {
        std::string buffer;
        buffer.reserve(256);
        llvm::raw_string_ostream os(buffer);
        debug(os, tree.getRoot(), /*showCoverage=*/false);

        llvm::StringRef view = os.str();
        for(auto kind: kinds) {
            auto strRepr = kind.asStringRef();
            auto pos = view.find(strRepr);
            EXPECT_NE(pos, llvm::StringRef::npos);
            view = view.ltrim().drop_front(strRepr.size());
        }
    }

    // void inspect(const SelectionBuilder& builder) {
    //     auto lk = builder.left->kind();
    //     auto rk = builder.right->kind();
    //     llvm::outs() << "left: " << lk << "\n";
    //     llvm::outs() << "right: " << rk << "\n";
    // }
};

using namespace clang;

template <typename... Ts>
std::array<ASTNodeKind, sizeof...(Ts)> makeNodeSequence() {
    return {ASTNodeKind::getFromNodeKind<Ts>()...};
}

TEST(Selection, SingleStmt) {
    const char* code = R"cpp(
namespace test {
    int f() {
        $(stmt_begin)int x = 1;$(stmt_end)
        return 0;
    }
}
)cpp";

    SelectionTester tx("main.cpp", code);
    tx.run();

    auto& info = tx.info;

    uint32_t begin = tx.getOffsetAt("stmt_begin");
    uint32_t end = tx.getOffsetAt("stmt_end");

    auto tokens = info.tokBuf().spelledTokens(info.srcMgr().getMainFileID());
    auto [left, right] = SelectionBuilder::selectionBound(tokens, {begin, end}, info.srcMgr());

    EXPECT_EQ(left->kind(), clang::tok::kw_int);
    EXPECT_EQ(right->kind(), clang::tok::semi);

    SelectionBuilder builder(left, right, info.context(), info.tokBuf());
    auto tree = builder.build();
    debug(tree);

    auto kinds = makeNodeSequence<NamespaceDecl, FunctionDecl, CompoundStmt, DeclStmt>();
    tx.expectPreorderSequence(tree, kinds);
}

TEST(Selection, MultiStmt) {
    const char* code = R"cpp(
namespace test {
    int f() {
        $(multi_begin)int x = 1;
        int y = x + 1;
        if (y) { x -= 1; }
        $(multi_end)
        return 0;
    }
}
)cpp";

    SelectionTester tx("main.cpp", code);
    tx.run();

    auto& info = tx.info;

    uint32_t begin = tx.getOffsetAt("multi_begin");
    uint32_t end = tx.getOffsetAt("multi_end");

    auto tokens = info.tokBuf().spelledTokens(info.srcMgr().getMainFileID());
    auto [left, right] = SelectionBuilder::selectionBound(tokens, {begin, end}, info.srcMgr());

    EXPECT_EQ(left->kind(), clang::tok::kw_int);
    EXPECT_EQ(right->kind(), clang::tok::r_brace);

    SelectionBuilder builder(left, right, info.context(), info.tokBuf());
    auto tree = builder.build();
    debug(tree);

    auto kinds =
        makeNodeSequence<NamespaceDecl, FunctionDecl, CompoundStmt, DeclStmt, DeclStmt, IfStmt>();
    tx.expectPreorderSequence(tree, kinds);
}

}  // namespace

}  // namespace clice
