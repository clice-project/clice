#include "src/AST/Selection.cpp"
#include "Server/SourceConverter.h"

#include "Test/CTest.h"

namespace clice {

namespace testing {
namespace {

using OffsetRange = std::pair<std::uint32_t, std::uint32_t>;

OffsetRange takeWholeFile(ASTInfo& info) {
    auto& src = info.srcMgr();
    auto fileID = src.getMainFileID();
    auto begin = src.getFileOffset(src.getLocForStartOfFile(fileID));
    auto end = src.getFileOffset(src.getLocForEndOfFile(fileID));
    return {begin, end};
}

void debug(llvm::raw_ostream& os,
           const SelectionTree::Node* node,
           bool showCoverage = true,
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
};

using namespace clang;

template <typename... Ts>
std::array<ASTNodeKind, sizeof...(Ts)> makeNodeSequence() {
    return {ASTNodeKind::getFromNodeKind<Ts>()...};
}

TEST(Selection, VarDeclSelectionBoundary) {
    const char* code = R"cpp(
$(b1)int xxx$(b2)yyy$(e1) = 1$(e2);$(e3)
)cpp";

    SelectionTester tx("main.cpp", code);
    tx.run();

    std::vector<SelectionBuilder::OffsetPair> selects;
    for(int begin = 1; begin <= 2; begin++) {
        for(int end = 1; end <= 3; end++) {
            uint32_t bp = tx.getOffsetAt(std::format("b{}", begin));
            uint32_t ep = tx.getOffsetAt(std::format("e{}", end));
            selects.push_back({bp, ep});
        }
    }

    auto& info = tx.info;
    auto tokens = info->tokBuf().spelledTokens(info->srcMgr().getMainFileID());
    for(auto& [begin, end]: selects) {
        auto [left, right] = SelectionBuilder::selectionBound(tokens, {begin, end}, info->srcMgr());

        SelectionBuilder builder(left, right, info->context(), info->tokBuf());
        auto tree = builder.build();
        // debug(tree);

        auto kinds = makeNodeSequence<VarDecl>();
        tx.expectPreorderSequence(tree, kinds);
    }
}

TEST(Selection, ParmVarDeclBoundary) {
    const char* code = R"cpp(
void f($(b1)int xxx$(b2)yyy$(e1) = 1$(e2)) {}
)cpp";

    SelectionTester tx("main.cpp", code);
    tx.run();

    std::vector<SelectionBuilder::OffsetPair> selects;
    for(int begin = 1; begin <= 2; begin++) {
        for(int end = 1; end <= 2; end++) {
            uint32_t bp = tx.getOffsetAt(std::format("b{}", begin));
            uint32_t ep = tx.getOffsetAt(std::format("e{}", end));
            selects.push_back({bp, ep});
        }
    }

    auto& info = tx.info;
    auto tokens = info->tokBuf().spelledTokens(info->srcMgr().getMainFileID());
    for(auto& [begin, end]: selects) {
        auto [left, right] = SelectionBuilder::selectionBound(tokens, {begin, end}, info->srcMgr());

        SelectionBuilder builder(left, right, info->context(), info->tokBuf());
        auto tree = builder.build();
        // debug(tree);

        auto kinds = makeNodeSequence<ParmVarDecl>();
        tx.expectPreorderSequence(tree, kinds);
    }
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

    auto tokens = info->tokBuf().spelledTokens(info->srcMgr().getMainFileID());
    auto [left, right] = SelectionBuilder::selectionBound(tokens, {begin, end}, info->srcMgr());

    EXPECT_EQ(left->kind(), clang::tok::kw_int);
    EXPECT_EQ(right->kind(), clang::tok::semi);

    SelectionBuilder builder(left, right, info->context(), info->tokBuf());
    auto tree = builder.build();
    // debug(tree);

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

    auto tokens = info->tokBuf().spelledTokens(info->srcMgr().getMainFileID());
    auto [left, right] = SelectionBuilder::selectionBound(tokens, {begin, end}, info->srcMgr());

    EXPECT_EQ(left->kind(), clang::tok::kw_int);
    EXPECT_EQ(right->kind(), clang::tok::r_brace);

    SelectionBuilder builder(left, right, info->context(), info->tokBuf());
    auto tree = builder.build();
    // debug(tree);

    auto kinds =
        makeNodeSequence<NamespaceDecl, FunctionDecl, CompoundStmt, DeclStmt, DeclStmt, IfStmt>();
    tx.expectPreorderSequence(tree, kinds);
}

TEST(Selection, EntireClass) {
    const char* code = R"cpp(
namespace test{
$(class_begin)class Test {
    int x;
    int y;

    void f();
};$(class_end)
}
)cpp";

    SelectionTester tx("main.cpp", code);
    tx.run();

    auto& info = tx.info;

    uint32_t begin = tx.getOffsetAt("class_begin");
    uint32_t end = tx.getOffsetAt("class_end");

    auto tokens = info->tokBuf().spelledTokens(info->srcMgr().getMainFileID());
    auto [left, right] = SelectionBuilder::selectionBound(tokens, {begin, end}, info->srcMgr());

    EXPECT_EQ(left->kind(), clang::tok::kw_class);
    EXPECT_EQ(right->kind(), clang::tok::semi);

    SelectionBuilder builder(left, right, info->context(), info->tokBuf());
    auto tree = builder.build();
    // debug(tree);

    auto kinds = makeNodeSequence<NamespaceDecl, CXXRecordDecl>();
    tx.expectPreorderSequence(tree, kinds);
}

TEST(Selection, ClassField) {
    const char* code = R"cpp(
class Test {
    int $(begin)x$(end);
    int y;
};
)cpp";

    SelectionTester tx("main.cpp", code);
    tx.run();

    auto& info = tx.info;

    uint32_t begin = tx.getOffsetAt("begin");
    uint32_t end = tx.getOffsetAt("end");

    auto tokens = info->tokBuf().spelledTokens(info->srcMgr().getMainFileID());
    auto [left, right] = SelectionBuilder::selectionBound(tokens, {begin, end}, info->srcMgr());

    EXPECT_EQ(left->kind(), clang::tok::identifier);
    EXPECT_EQ(right->kind(), clang::tok::identifier);

    SelectionBuilder builder(left, right, info->context(), info->tokBuf());
    auto tree = builder.build();
    // debug(tree);

    auto kinds = makeNodeSequence<CXXRecordDecl, FieldDecl>();
    tx.expectPreorderSequence(tree, kinds);
}

TEST(Selection, IfCondExpr) {
    const char* code = R"cpp(
void f(int& x){
    if ($(begin1)x $(begin2)==$(end2) 1$(end1)) {}   
}
)cpp";

    SelectionTester tx("main.cpp", code);
    tx.run();

    auto& info = tx.info;

    {
        uint32_t begin = tx.getOffsetAt("begin1");
        uint32_t end = tx.getOffsetAt("end1");

        auto tokens = info->tokBuf().spelledTokens(info->srcMgr().getMainFileID());
        auto [left, right] = SelectionBuilder::selectionBound(tokens, {begin, end}, info->srcMgr());

        EXPECT_EQ(left->kind(), clang::tok::identifier);
        EXPECT_EQ(right->kind(), clang::tok::numeric_constant);

        SelectionBuilder builder(left, right, info->context(), info->tokBuf());
        auto tree = builder.build();
        // debug(tree);

        auto kinds = makeNodeSequence<FunctionDecl,
                                      CompoundStmt,
                                      IfStmt,
                                      BinaryOperator,
                                      ImplicitCastExpr,
                                      IntegerLiteral>();
        tx.expectPreorderSequence(tree, kinds);
    }

    {
        uint32_t begin = tx.getOffsetAt("begin2");
        uint32_t end = tx.getOffsetAt("end2");

        auto tokens = info->tokBuf().spelledTokens(info->srcMgr().getMainFileID());
        auto [left, right] = SelectionBuilder::selectionBound(tokens, {begin, end}, info->srcMgr());

        auto lk = left->kind();
        auto rk = right->kind();

        EXPECT_EQ(left->kind(), clang::tok::equalequal);
        EXPECT_EQ(right->kind(), clang::tok::equalequal);

        SelectionBuilder builder(left, right, info->context(), info->tokBuf());
        auto tree = builder.build();
        // debug(tree);

        auto kinds = makeNodeSequence<FunctionDecl, CompoundStmt, IfStmt, BinaryOperator>();
        tx.expectPreorderSequence(tree, kinds);
    }
}

TEST(Selection, ClassMethod) {
    const char* code = R"cpp(
class Test {
    $(b1)void $(b2)f(int x, int y) $(b3){$(e1)
        int z = x + y;$(e2)
    }$(e3)
};
)cpp";

    SelectionTester tx("main.cpp", code);
    tx.run();

    {  // {b1, b2} X {e1, e2, e3}
        std::vector<SelectionBuilder::OffsetPair> b12_e123;
        for(int begin = 1; begin <= 2; begin++) {
            for(int end = 1; end <= 3; end++) {
                uint32_t bp = tx.getOffsetAt(std::format("b{}", begin));
                uint32_t ep = tx.getOffsetAt(std::format("e{}", end));
                b12_e123.push_back({bp, ep});
            }
        }

        auto& info = tx.info;
        auto tokens = info->tokBuf().spelledTokens(info->srcMgr().getMainFileID());
        for(auto& [begin, end]: b12_e123) {
            auto [left, right] =
                SelectionBuilder::selectionBound(tokens, {begin, end}, info->srcMgr());

            SelectionBuilder builder(left, right, info->context(), info->tokBuf());
            auto tree = builder.build();
            // debug(tree);

            auto kinds = makeNodeSequence<CXXRecordDecl,
                                          CXXMethodDecl,
                                          FunctionProtoTypeLoc,
                                          CompoundStmt>();
            tx.expectPreorderSequence(tree, kinds);
        }
    }

    {
        // {b3} X {e1, e2, e3}
        std::vector<SelectionBuilder::OffsetPair> b3_e123;
        for(int begin = 3; begin <= 3; begin++) {
            for(int end = 1; end <= 3; end++) {
                uint32_t bp = tx.getOffsetAt(std::format("b{}", begin));
                uint32_t ep = tx.getOffsetAt(std::format("e{}", end));
                b3_e123.push_back({bp, ep});
            }
        }

        auto& info = tx.info;
        auto tokens = info->tokBuf().spelledTokens(info->srcMgr().getMainFileID());
        for(auto& [begin, end]: b3_e123) {
            auto [left, right] =
                SelectionBuilder::selectionBound(tokens, {begin, end}, info->srcMgr());

            SelectionBuilder builder(left, right, info->context(), info->tokBuf());
            auto tree = builder.build();
            // debug(tree);

            // for b3 X e1, only care about the method body.
            auto kinds = makeNodeSequence<CXXRecordDecl, CXXMethodDecl, CompoundStmt>();

            // for b3 X e23, ther is also a partial coverage of DeclStmt, but don't check it here.
            // auto kinds = makeNodeSequence<CXXRecordDecl, CXXMethodDecl, CompoundStmt,
            // DeclStmt>();

            tx.expectPreorderSequence(tree, kinds);
        }
    }
}

}  // namespace

}  // namespace testing

}  // namespace clice
