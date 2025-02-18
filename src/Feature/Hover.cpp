#include "Feature/Hover.h"

#include "AST/Selection.h"
#include "Compiler/AST.h"
#include "Support/FileSystem.h"

#include <clang/Lex/LiteralSupport.h>
#include <clang/AST/DeclVisitor.h>

#ifdef _WIN32
#include <variant>
#include <format>
#endif

template <class... Ts>
struct Match : Ts... {
    using Ts::operator()...;
};

namespace clice {

namespace {

struct HoverBase {
    /// The kind of the symbol.
    SymbolKind symbol;

    /// The name of given item (declaration or variable name or something else).
    std::string name;

    /// The text of source code of given declaration.
    std::string source;

    size_t estimated_size() const {
        return name.size() + source.size();
    }
};

struct WithDocument {
    std::string document;

    size_t estimated_size() const {
        return document.size();
    }
};

struct WithScope {
    /// For class/struct/enum in some namespace, like "NamespaceA::NamespaceA::", maybe empty.
    std::string namespac;

    /// For nested class, like "StructA::StructB::", maybe empty.
    std::string local;

    size_t estimated_size() const {
        return namespac.size() + local.size();
    }
};

struct Namespace : HoverBase, WithScope {
    size_t estimated_size() const {
        return HoverBase::estimated_size() + WithScope::estimated_size();
    }
};

/// Memory layout information of a class/struct/field.
struct MemoryLayout {

    bool isBitField = false;

    // bits if isBitField is true, otherwise in bytes
    uint8_t padding;

    // bits if isBitField is true, otherwise in bytes
    uint16_t size;

    // bits if isBitField is true, otherwise in bytes
    uint16_t offset;

    // always in bytes
    uint16_t align;
};

struct PrettyType {
    /// The type of the item in a more human-readable format.
    std::string type;

    /// Desugared type of the item. （aka `SomeDesugaredType`)
    std::string akaType;

    size_t estimated_size() const {
        return type.size() + akaType.size();
    }
};

struct Field : WithDocument, WithScope, MemoryLayout {

    PrettyType type;

    std::string name;

    clang::AccessSpecifier access;

    size_t estimated_size() const {
        return WithDocument::estimated_size() + WithScope::estimated_size() + sizeof(MemoryLayout) +
               type.estimated_size() + name.size();
    }
};

template <typename T>
size_t estimated_size(const std::vector<T>& xs) {
    return std::ranges::fold_left(xs, 0, [](size_t acc, const T& x) {
        return acc + x.estimated_size();
    });
}

struct Enum : HoverBase, WithDocument, WithScope {

    struct EnumItem {
        std::string name;
        std::string value;

        size_t estimated_size() const {
            return name.size() + value.size();
        }
    };

    /// underlying type of the enum, e.g. "int", "unsigned int", "long long"
    std::string implType;

    std::vector<EnumItem> items;

    bool isScoped;

    size_t estimated_size() const {
        return HoverBase::estimated_size() + WithDocument::estimated_size() +
               WithScope::estimated_size() + implType.size() + estimated_size<EnumItem>(items);
    }
};

/// TODO: macro parameters ?
/// Represents parameters of a function, a template or a macro.
/// For example:
/// - void foo(ParamType Name = DefaultValue)
/// - #define FOO(Name)
/// - template <ParamType Name = DefaultType> class Foo {};
struct Param {
    /// In case the template parameter, kind is `Type` parameter.
    SymbolKind kind;

    /// The printable parameter type, e.g. "int", or "typename" (in
    /// TemplateParameters)
    PrettyType type;

    /// Empty if is unnamed parameters.
    std::string name;

    /// Empty if no default is provided.
    std::string defaultParaName;

    size_t estimated_size() const {
        return type.estimated_size() + name.size() + defaultParaName.size();
    }
};

struct Record : HoverBase, WithDocument, WithScope, MemoryLayout {
    std::vector<Field> fields;

    std::vector<Param> templateParams;

    size_t estimated_size() const {
        return HoverBase::estimated_size() + WithDocument::estimated_size() +
               WithScope::estimated_size() + estimated_size<Field>(fields) +
               estimated_size<Param>(templateParams);
    }
};

struct Fn : HoverBase, WithDocument, WithScope {
    PrettyType retType;

    std::vector<Param> params;

    std::vector<Param> templateParams;

    bool isConstexpr = false;
    bool isConsteval = false;
    bool isInlined = false;
    bool isStatic = false;
    bool isPureVirtual = false;
    bool isOverloadedOperator = false;
    bool isExternallyVisible = false;
    bool isDeprecated = false;

    /// Non empty if isDeprecated is true.
    std::string deprecateReason;

    size_t estimated_size() const {
        return HoverBase::estimated_size() + WithDocument::estimated_size() +
               WithScope::estimated_size() + retType.estimated_size() +
               estimated_size<Param>(params) + estimated_size<Param>(templateParams);
    }
};

struct Var : HoverBase, WithDocument, WithScope {
    PrettyType type;

    uint16_t size;
    uint16_t align;

    bool isConstexpr = false;
    bool isFileScope = false;
    bool isLocal = false;
    bool isStaticLocal = false;
    bool isExtern = false;
    bool isDeprecated = false;

    /// Non empty if the variable has a constexpr evaluated value.;
    // std::string value;

    // Non empty if isDeprecated is true.
    std::string deprecateReason;

    size_t estimated_size() const {
        return HoverBase::estimated_size() + WithDocument::estimated_size() +
               WithScope::estimated_size() + type.estimated_size();
    }
};

/// Include directive.
struct Header : HoverBase {

    std::string absPath;

    /// TODO: symbols provided by the header
    /// std::vector<std::string> provides;

    size_t estimated_size() const {
        return HoverBase::estimated_size() + absPath.size();
    }
};

struct Numeric {
    SymbolKind symbol = SymbolKind::Number;

    llvm::StringRef rawText;

    std::variant<llvm::APInt, llvm::APFloat> value;

    size_t estimated_size() const {
        return 16;
    }
};

struct Keyword {
    SymbolKind symbol = SymbolKind::Keyword;

    clang::tok::TokenKind tkkind;

    llvm::StringRef spelling;

    constexpr static std::string_view CppRefKeywordBaseUrl =
        "https://en.cppreference.com/w/cpp/keyword/";

    static std::string cpprefLink(clang::tok::TokenKind keyword) {
        std::string url{CppRefKeywordBaseUrl};
        url += clang::tok::getTokenName(keyword);
        return url;
    }

    size_t estimated_size() const {
        return 64;
    }
};

struct Literal {
    SymbolKind symbol = SymbolKind::String;

    clang::tok::TokenKind kind;

    std::string content;

    /// User-defined suffix of the literal, e.g. "_s" in "123"_s, maybe empty.
    std::string udSuffix;

    size_t estimated_size() const {
        return content.size() + 32;
    }

    static std::string_view stringLiteralKindName(clang::tok::TokenKind kind) {
        switch(kind) {
            case clang::tok::char_constant: return "char_constant";
            case clang::tok::string_literal: return "string_literal";
            case clang::tok::wide_string_literal: return "wide_string_literal";
            case clang::tok::utf8_string_literal: return "utf8_string_literal";
            case clang::tok::utf16_string_literal: return "utf16_string_literal";
            case clang::tok::utf32_string_literal: return "utf32_string_literal";
            // case clang::tok::binary_data: return "binary_data";
            default: return "unknown";
        }
    }
};

struct Expression : HoverBase {

    PrettyType type;

    /// TODO:
    std::string evaluated;

    size_t estimated_size() const {
        return HoverBase::estimated_size() + evaluated.size() + type.estimated_size();
    }
};

/// Make HoverInfo default constructible.
struct Empty : std::monostate {
    size_t estimated_size() const {
        return 0;
    }
};

/// Use variant to store different types of hover information to reduce the memory usage.
struct HoverInfo :
    public std::variant<Empty,
                        Namespace,
                        Record,
                        Field,
                        Enum,
                        Fn,
                        Var,
                        Header,
                        Numeric,
                        Literal,
                        Expression,
                        Keyword> {
    /// Return the estimated size of the hover information in bytes.
    size_t estimated_size() const {
        auto size_counter = Match{
            [](const auto& x) { return x.estimated_size(); },
        };
        return std::visit(size_counter, *this);
    }
};

struct DeclHoverBuilder : public clang::ConstDeclVisitor<DeclHoverBuilder, void> {
    using Base = clang::ConstDeclVisitor<DeclHoverBuilder, void>;

    const config::HoverOption option;

    /// State used to build bitfield memorylayout information.
    uint32_t continousBitFieldBits = 0;

    HoverInfo hover;

    static void recFillScope(const clang::DeclContext* DC, WithScope& scope) {
        if(DC->isTranslationUnit())
            return;

        if(auto ND = llvm::dyn_cast<clang::NamespaceDecl>(DC)) {
            auto name = ND->getName();
            if(ND->isAnonymousNamespace()) {
                recFillScope(ND->getDeclContext(), scope);
                scope.namespac += "(anonymous)";
                scope.namespac += name;
            } else if(ND->isInline()) {
                /// FIXME: Should we add "(inline)" to the namespace name ?
                recFillScope(ND->getDeclContext(), scope);
                scope.namespac += "(inline)";
                scope.namespac = name;
            } else {
                recFillScope(ND->getDeclContext(), scope);
                scope.namespac += name;
            }
            scope.namespac += "::";
        } else if(auto RD = llvm::dyn_cast<clang::RecordDecl>(DC)) {
            recFillScope(RD->getDeclContext(), scope);
            scope.local += RD->getName(), scope.local += "::";
        } else if(auto FD = llvm::dyn_cast<clang::FunctionDecl>(DC)) {
            recFillScope(FD->getDeclContext(), scope);
            scope.local += FD->getNameAsString(), scope.local += "::";
        }
    }

    static void fillScope(const clang::DeclContext* DC, WithScope& scope) {
        recFillScope(DC, scope);

        for(auto& lens: {std::ref(scope.local), std::ref(scope.namespac)}) {
            // remove tailing "::"
            if(auto& ref = lens.get(); ref.ends_with("::"))
                ref.pop_back(), ref.pop_back();
        }
    }

    void VisitNamespaceDecl(const clang::NamespaceDecl* ND) {
        Namespace ns;
        ns.symbol = SymbolKind::Namespace;
        fillScope(ND, ns);

        ns.source = "<TODO: source code>";
        hover.emplace<Namespace>(std::move(ns));
    }

    void fillMemoryLayout(const clang::RecordDecl* FD, MemoryLayout& lay) {
        clang::ASTContext& ctx = FD->getASTContext();
        clang::QualType QT = ctx.getRecordType(FD);

        lay.align = ctx.getTypeAlignInChars(QT).getQuantity();
        lay.size = ctx.getTypeSizeInChars(QT).getQuantity();
    }

    void fillRecord(const clang::RecordDecl* RD, Record& rc) {
        rc.symbol = RD->isStruct() ? SymbolKind::Struct : SymbolKind::Class;
        rc.name = RD->getNameAsString();

        fillMemoryLayout(RD, rc);
        fillScope(RD->getDeclContext(), rc);

        rc.source = "<TODO: source code>";
        rc.document = "<TODO: document>";

        for(auto FD: RD->fields()) {
            Field fd;
            fillField(FD, fd);
            rc.fields.push_back(std::move(fd));
        }
    }

    void VisitRecordDecl(const clang::RecordDecl* RD) {
        Record rc;
        fillRecord(RD, rc);

        hover.emplace<Record>(std::move(rc));
    }

    void VisitClassTemplateDecl(const clang::ClassTemplateDecl* TD) {
        Record rc;
        for(const auto& tpara: *TD->getTemplateParameters()) {
            Param p;
            p.name = tpara->getName();
            rc.templateParams.push_back(std::move(p));
        }

        fillRecord(TD->getTemplatedDecl(), rc);
        hover.emplace<Record>(std::move(rc));
        VisitRecordDecl(llvm::dyn_cast<clang::RecordDecl>(TD->getTemplatedDecl()));
    }

    void VisitClassTemplateSpecializationDecl(const clang::ClassTemplateSpecializationDecl* TD) {
        VisitClassTemplateDecl(TD->getSpecializedTemplate());
    }

    void fillMemoryLayout(const clang::FieldDecl* FD,
                          const clang::RecordDecl* RD,
                          MemoryLayout& lay) {
        clang::QualType QT = FD->getType();
        clang::ASTContext& ctx = RD->getASTContext();

        lay.isBitField = FD->isBitField();

        auto bitsOffset = ctx.getFieldOffset(FD);
        lay.offset = lay.isBitField ? bitsOffset : bitsOffset / 8;
        lay.align = ctx.getTypeAlignInChars(QT).getQuantity();

        if(lay.isBitField) {
            /// lay.size = FD->getBitWidthValue(ctx);
            continousBitFieldBits += lay.size;
            if(continousBitFieldBits > 8) {
                lay.padding = lay.align - continousBitFieldBits % lay.align;
            }
        } else {
            lay.size = ctx.getTypeSizeInChars(QT).getQuantity();
            continousBitFieldBits = 0;
            lay.padding = lay.align - (lay.offset % lay.align) % lay.align;
        }
    }

    void fillField(const clang::FieldDecl* FD, Field& field) {
        field.access = FD->getAccess();
        field.name = FD->getNameAsString();

        auto RD = llvm::dyn_cast<clang::RecordDecl>(FD->getDeclContext());
        fillScope(RD, field);
        fillMemoryLayout(FD, RD, field);

        auto ty = FD->getType();
        auto canonicalTy = ty.getCanonicalType();  // ignore typedef

        if(ty == canonicalTy) {
            field.type.type = ty.getAsString();
        } else {
            field.type.type = ty.getAsString();
            field.type.akaType = canonicalTy.getAsString();
        }
    }

    void VisitFieldDecl(const clang::FieldDecl* FD) {
        Field field;
        fillField(FD, field);
        hover.emplace<Field>(std::move(field));
    }

    void VisitEnumDecl(const clang::EnumDecl* ED) {
        using RK = enum config::MemoryLayoutRenderKind;

        Enum enm;
        enm.symbol = SymbolKind::Enum;
        enm.name = ED->getName();
        enm.isScoped = ED->isScoped();
        enm.implType = ED->getIntegerType().getAsString();
        fillScope(ED->getDeclContext(), enm);

        enm.source = "<TODO: source code>";
        enm.document = "<TODO: document>";

        for(const auto ECD: ED->enumerators()) {
            llvm::SmallString<64> buffer;

            auto val = ECD->getInitVal();
            auto kind = option.memoryLayoutRenderKind;
            if(kind == RK::Both || kind == RK::Decimal) {
                val.toString(buffer, 10);
            }

            if(kind == RK::Both || kind == RK::Hexadecimal) {
                bool parent = !buffer.empty();
                if(parent)
                    buffer += " (";
                buffer += "0x";
                val.toString(buffer, 16);
                if(parent)
                    buffer += ")";
            }

            enm.items.push_back(Enum::EnumItem{
                .name = ECD->getName().str(),
                .value = buffer.str().str(),
            });
        }

        hover.emplace<Enum>(std::move(enm));
    }

    static void fillFunction(const clang::FunctionDecl* FD, Fn& fn) {
        if(fn.symbol != SymbolKind::Method && fn.symbol != SymbolKind::Operator) {
            fn.symbol = SymbolKind::Function;
        }

        fn.name = FD->getNameAsString();
        fn.isConstexpr = FD->isConstexpr();
        fn.isConsteval = FD->isConsteval();
        fn.isInlined = FD->isInlined();
        fn.isStatic = FD->isStatic();
        fn.isOverloadedOperator = FD->isOverloadedOperator();
        fn.isPureVirtual = FD->isPureVirtual();
        fn.isExternallyVisible = FD->isExternallyVisible();
        fn.isDeprecated = FD->isDeprecated(&fn.deprecateReason);

        fillScope(FD->getDeclContext(), fn);

        auto retType = FD->getReturnType();
        auto canonicalRetType = retType.getCanonicalType();
        if(retType == canonicalRetType) {
            fn.retType.type = retType.getAsString();
        } else {
            fn.retType.type = retType.getAsString();
            fn.retType.akaType = canonicalRetType.getAsString();
        }

        for(auto param: FD->parameters()) {
            Param p;
            p.name = param->getName();

            auto qty = param->getType();
            p.type.type = qty.getAsString();
            if(auto cqty = qty.getCanonicalType(); cqty != qty) {
                p.type.akaType = cqty.getAsString();
            }

            fn.params.push_back(std::move(p));
        }

        fn.source = "<TODO: source code>";
        fn.document = "<TODO: document>";
    }

    void VisitFunctionDecl(const clang::FunctionDecl* FD) {
        Fn fn;
        fillFunction(FD, fn);
        hover.emplace<Fn>(std::move(fn));
    }

    void VisitFunctionTemplateDecl(const clang::FunctionTemplateDecl* TD) {
        Fn fn;

        for(auto tpara: *TD->getTemplateParameters()) {
            Param p;
            p.kind = SymbolKind::Type;
            p.name = tpara->getName();
            fn.templateParams.push_back(std::move(p));
        }

        fillFunction(TD->getAsFunction(), fn);
        hover.emplace<Fn>(std::move(fn));
    }

    void VisitTemplateTypeParmDecl(const clang::TemplateTypeParmDecl* TTPD) {
        Var var;

        var.symbol = SymbolKind::Type;
        var.name = TTPD->getName();
        if(var.name.empty())
            var.name = "(unnamed)";

        fillScope(TTPD->getDeclContext(), var);
        var.type.type = TTPD->getNameAsString();

        hover.emplace<Var>(std::move(var));
    }

    void VisitNonTypeTemplateParmDecl(const clang::NonTypeTemplateParmDecl* NTPD) {
        Var var;

        var.symbol = SymbolKind::Parameter;
        var.name = NTPD->getName();
        if(var.name.empty())
            var.name = "(unnamed)";

        fillScope(NTPD->getDeclContext(), var);

        auto qty = NTPD->getType();
        var.type.type = qty.getAsString();
        if(auto cqty = qty.getCanonicalType().getAsString(); cqty != var.type.type) {
            var.type.akaType = std::move(cqty);
        }

        var.document = "<TODO: document>";
        var.source = "<TODO: source code>";

        hover.emplace<Var>(std::move(var));
    }

    void VisitTemplateTemplateParmDecl(const clang::TemplateTemplateParmDecl* TTPD) {
        Var var;

        var.symbol = SymbolKind::Type;
        var.name = TTPD->getName();
        if(var.name.empty())
            var.name = "(unnamed)";

        fillScope(TTPD->getDeclContext(), var);
        var.type.type = TTPD->getNameAsString();

        hover.emplace<Var>(std::move(var));
    }

    void VisitCXXMethodDecl(const clang::CXXMethodDecl* MD) {
        Fn fn;
        fn.symbol = SymbolKind::Method;
        fillFunction(MD, fn);
        hover.emplace<Fn>(std::move(fn));
    }

    static void fillVarInfo(const clang::VarDecl* VD, Var& var) {
        var.name = VD->getName();
        if(var.name.empty())
            var.name = "(unnamed)";

        var.isConstexpr = VD->isConstexpr();
        var.isFileScope = VD->isFileVarDecl();
        var.isLocal = VD->isLocalVarDecl();
        var.isStaticLocal = VD->isStaticLocal();
        var.isExtern = VD->isLocalExternDecl();
        var.isDeprecated = VD->isDeprecated(&var.deprecateReason);
        if(!var.isDeprecated && var.name.starts_with("_")) {
            var.isDeprecated = true;
            var.deprecateReason = "Manually marked as throwaway variable";
        }

        fillScope(VD->getDeclContext(), var);

        auto qty = VD->getType();
        var.type.type = qty.getAsString();
        if(auto cqty = qty.getCanonicalType().getAsString(); cqty != var.type.type) {
            var.type.akaType = std::move(cqty);
        }

        if(!qty->isDependentType()) {
            auto& ctx = VD->getASTContext();
            var.size = ctx.getTypeSizeInChars(qty).getQuantity();
            var.align = ctx.getTypeAlignInChars(qty).getQuantity();
        }

        var.document = "<TODO: document>";
        var.source = "<TODO: source code>";
    }

    void VisitVarDecl(const clang::VarDecl* VD) {
        Var var;
        var.symbol = SymbolKind::Variable;
        fillVarInfo(VD, var);
        hover.emplace<Var>(std::move(var));
    }

    void VisitParmVarDecl(const clang::ParmVarDecl* PVD) {
        Var var;
        var.symbol = SymbolKind::Parameter;
        fillVarInfo(PVD, var);
        hover.emplace<Var>(std::move(var));
    }

    static HoverInfo build(const clang::Decl* decl, const config::HoverOption& option) {
        assert(decl && "Must be non-null pointer");
        DeclHoverBuilder builder{.option = option};
        builder.Visit(decl);
        return std::move(builder.hover);
    }
};

using Token = clang::syntax::Token;
using namespace clang::tok;

llvm::SmallVector<Token> pickBestToken(llvm::ArrayRef<Token>& touching) {
    constexpr auto ranker = [](const Token& tk) -> uint32_t {
        auto kind = tk.kind();
        if(isAnyIdentifier(kind))
            return 10;

        if(llvm::is_contained({kw_auto, kw_decltype}, kind))
            return 9;

        if(isStringLiteral(kind))
            return 7;

        auto prefix_ops = {
            l_square,
            r_square,
            star,
            minus,
            exclaim,
            numeric_constant,
            clang::tok::pipe,
        };
        if(llvm::is_contained(prefix_ops, kind))
            return 6;

        // keyword or function call.
        if(getKeywordSpelling(kind) || llvm::is_contained({l_paren, r_paren}, kind))
            return 5;

        if(getPunctuatorSpelling(kind))
            return 0;

        return 1;
    };

    llvm::SmallVector<Token> ranked{touching};
    std::ranges::sort(ranked, [ranker](const Token& lhs, const Token& rhs) {
        return ranker(lhs) > ranker(rhs);
    });
    return ranked;
}

struct ExprHoverBuilder {

    using Node = SelectionTree::Node;

    template <typename... Ts>
    struct Accept :
        public std::variant<std::nullptr_t, std::add_pointer_t<std::add_const_t<Ts>>...> {

        using Cases = std::variant<std::nullptr_t, std::add_pointer_t<std::add_const_t<Ts>>...>;

        const Node* deepest = nullptr;

        template <typename P>
        void accept(const Node* node) {
            if(auto ptr = node->dynNode.get<P>()) {
                this->template emplace<std::add_pointer_t<std::add_const_t<P>>>(ptr);
                deepest = node;
            }
        }

        bool accept(const Node* node) {
            // Return true to get the deepest node.
            return (accept<Ts>(node), ...), true;
        }
    };

    using PreciseExprMatcher = Accept<clang::DeclRefExpr,
                                      clang::CXXMemberCallExpr,
                                      clang::CallExpr,
                                      clang::CXXNamedCastExpr>;

    const SelectionTree& tree;
    ASTInfo& AST;
    const config::HoverOption& option;

    /// The final matched expression node by `PreciseExprMatcher`.
    const Node* target = nullptr;

    using Res = std::optional<HoverInfo>;

    Res operator() (const clang::DeclRefExpr* DR) const {
        return DeclHoverBuilder::build(DR->getDecl(), option);
    }

    Res operator() (const clang::CXXMemberCallExpr* MC) const {
        return DeclHoverBuilder::build(MC->getCalleeDecl(), option);
    }

    Res operator() (const clang::CallExpr* C) const {
        return DeclHoverBuilder::build(C->getCalleeDecl(), option);
    }

    Res operator() (const clang::CXXNamedCastExpr* NC) const {
        auto dest = NC->getTypeAsWritten();
        if(dest->isFundamentalType()) {
            return std::nullopt;
        }

        Expression expr;
        expr.symbol = SymbolKind::Variable;
        expr.name = NC->getStmtClassName();

        expr.type.type = dest.getAsString();
        if(auto qty = dest.getCanonicalType(); qty != dest) {
            expr.type.akaType = qty.getAsString();
        }

        expr.source = "<TODO: source code>";
        return HoverInfo{std::move(expr)};
    }

    // By default, return empty.
    template <typename O>
    Res operator() (O) const {
        return std::nullopt;
    }

    static Res build(ASTInfo& AST, const SelectionTree& tree, const config::HoverOption& option) {
        PreciseExprMatcher expr;
        if(!tree.walkDfs([&expr](const Node* node) { return expr.accept(node); })) {
            return std::nullopt;
        }

        ExprHoverBuilder builder{tree, AST, option, expr.deepest};
        return std::visit(builder, expr);
    }
};

namespace hit {

std::optional<HoverInfo> header(llvm::ArrayRef<Include> includes, ASTInfo& AST, uint32_t line) {
    auto lineof = [&SM = AST.srcMgr()](const Include& inc) {
        return SM.getPresumedLineNumber(inc.location);
    };

    if(includes.empty() || lineof(includes.back()) < line) {
        return std::nullopt;
    }

    for(auto& inc: includes) {
        if(lineof(inc) != line)
            continue;

        Header ic;
        ic.symbol = SymbolKind::Header;
        ic.name = inc.fileName;
        ic.absPath = path::join(inc.searchPath, inc.relativePath);
        return HoverInfo{std::move(ic)};
    }

    return std::nullopt;
}

std::optional<HoverInfo> numeric(const Token& token, ASTInfo& AST) {
    if(auto kind = token.kind(); kind == numeric_constant) {
        llvm::StringRef text = token.text(AST.srcMgr());
        auto& Ctx = AST.context();
        clang::NumericLiteralParser parser(text,
                                           token.location(),
                                           AST.srcMgr(),
                                           Ctx.getLangOpts(),
                                           Ctx.getTargetInfo(),
                                           Ctx.getDiagnostics());
        llvm::APInt apint;
        if(!parser.GetIntegerValue(apint)) {
            return HoverInfo{
                Numeric{.rawText = text, .value = apint}
            };
        }

        llvm::APFloat apfloat{0.0};
        if(parser.GetFloatValue(apfloat, llvm::RoundingMode::NearestTiesToEven)) {
            return HoverInfo{
                Numeric{.rawText = text, .value = apfloat}
            };
        }

        std::string reason = std::format("Parse numeric literal failed, text: {}", text);
        llvm_unreachable(reason.c_str());
    }
    return std::nullopt;
}

std::optional<HoverInfo> keyword(const Token& token) {
    if(auto spelling = getKeywordSpelling(token.kind())) {
        return HoverInfo{
            Keyword{.tkkind = token.kind(), .spelling = spelling}
        };
    }
    return std::nullopt;
}

std::optional<HoverInfo> literal(const Token& token, ASTInfo& AST) {
    if(isStringLiteral(token.kind())) {
        auto& Ctx = AST.context();
        clang::Token raw;
        bool isFail = clang::Lexer::getRawToken(token.location(),
                                                raw,
                                                AST.srcMgr(),
                                                Ctx.getLangOpts(),
                                                /*IgnoreWhiteSpace=*/true);
        if(!isFail) {
            clang::StringLiteralParser parser(raw,
                                              AST.srcMgr(),
                                              Ctx.getLangOpts(),
                                              Ctx.getTargetInfo());
            auto text = parser.GetString();
            auto udsuffix = parser.getUDSuffix();
            return HoverInfo{
                Literal{.kind = token.kind(), .content = text.str(), .udSuffix = udsuffix.str()}
            };
        }
    }

    return std::nullopt;
}

std::optional<HoverInfo> deduced(const Token& token,
                                 ASTInfo& AST,
                                 const SelectionTree& tree,
                                 const config::HoverOption& option) {
    if(token.kind() != kw_auto && token.kind() != kw_decltype) {
        return std::nullopt;
    }

    const SelectionTree::Node* ctx = nullptr;
    auto findDeclContext = [&AST, &ctx](const SelectionTree::Node* node) -> bool {
        // `decltype(auto)` is `AutoTypeLoc`.
        if(auto AT = node->dynNode.get<clang::AutoTypeLoc>()) {
            ctx = node->parent;
            return false;
        }
        if(auto DT = node->dynNode.get<clang::DecltypeTypeLoc>()) {
            ctx = node->parent;
            return false;
        }
        return true;
    };

    if(tree.walkDfs(findDeclContext))
        return std::nullopt;

    if(auto dynKind = ctx->dynNode.getNodeKind();
       dynKind.isSame(clang::ASTNodeKind::getFromNodeKind<clang::FunctionProtoTypeLoc>())) {
        ctx = ctx->parent;
    }

    const clang::Decl* decl = ctx->dynNode.get<clang::Decl>();
    assert(decl && "Selected Node must be a valid pointer");

    return DeclHoverBuilder::build(decl, option);
}

std::optional<HoverInfo> declaration(const SelectionTree& tree, const config::HoverOption& option) {
    const clang::Decl* decl = nullptr;

    // Find the most inner declaration node.
    tree.walkDfs([&decl](const SelectionTree::Node* node) {
        if(auto D = node->dynNode.get<clang::Decl>())
            decl = D;
        return true;
    });

    if(!decl) {
        return std::nullopt;
    }

    return DeclHoverBuilder::build(decl, option);
}

std::optional<HoverInfo> expression(ASTInfo& AST,
                                    const SelectionTree& tree,
                                    const config::HoverOption& option) {
    return ExprHoverBuilder::build(AST, tree, option);
}

std::optional<HoverInfo> detect(const Token& token,
                                ASTInfo& AST,
                                const config::HoverOption& option) {
    auto cheap = hit::numeric(token, AST).or_else([&]() { return hit::literal(token, AST); });
    auto expensive = [&]() -> std::optional<HoverInfo> {
        const auto tree = SelectionTree::selectToken(token, AST.context(), AST.tokBuf());
        if(!tree.hasValue())
            return std::nullopt;

        return deduced(token, AST, tree, option)
            .or_else([&]() { return hit::expression(AST, tree, option); })
            .or_else([&]() { return hit::declaration(tree, option); })
            .or_else([&]() { return hit::keyword(token); });
    };

    // Try some cheap cases first to avoid construct a selection tree.
    return cheap.or_else(expensive);
}

}  // namespace hit

struct MarkdownPrinter {

    using Self = MarkdownPrinter;

    const config::HoverOption option;

    std::string buffer;

    /// Title
    constexpr static std::string_view H3 = "###";

    /// Horizontal Rules.
    constexpr static std::string_view HLine = "\n___\n";

    template <typename... Args>
    Self& v(std::format_string<Args...> fmt, Args&&... args) {
        std::format_to(std::back_inserter(buffer), fmt, std::forward<Args>(args)...);
        return *this;
    }

    template <typename... Args>
    Self& vif(bool cond, std::format_string<Args...> fmt, Args&&... args) {
        if(cond)
            std::format_to(std::back_inserter(buffer), fmt, std::forward<Args>(args)...);
        return *this;
    }

    /// Optional print the text if it's not empty.
    Self& o(std::format_string<std::string_view&> fmt, std::string_view text) {
        if(!(text.empty() || text == "_"))
            std::format_to(std::back_inserter(buffer), fmt, text);
        return *this;
    }

    /// Optional print the text if it's not empty.
    Self& oln(std::format_string<std::string_view&> fmt, std::string_view text) {
        if(!(text.empty() || text == "_")) {
            return std::format_to(std::back_inserter(buffer), fmt, text), ln();
        }
        return *this;
    }

    Self& ln() {
        buffer += '\n', buffer += '\n';
        return *this;
    }

    template <typename... Args>
    Self& vln(std::format_string<Args...> fmt, Args&&... args) {
        return v(fmt, std::forward<Args>(args)...).ln();
    }

    Self& hln() {
        while(buffer.ends_with("\n\n"))
            buffer.pop_back();

        if(!buffer.ends_with(HLine))
            buffer += HLine;
        return *this;
    }

    template <typename... Args>
    Self& vhln(std::format_string<Args...> fmt, Args&&... args) {
        return v(fmt, std::forward<Args>(args)...), hln();
    }

    template <typename T>
    Self& iter(const std::vector<T>& items, llvm::function_ref<void(const T&)> op) {
        for(auto& it: items) {
            op(it);
        }
        return *this;
    }

    void operator() (const Namespace& sp) {
        // clang-format off
        vhln("{} {} `{}{}`", H3, sp.symbol.name(), sp.namespac, sp.name)
        .vln("{}", sp.source);
        // clang-format on
    }

    Self& title(const HoverBase& bs) {
        return v("{} {} `{}`", H3, bs.symbol.name(), bs.name);
    }

    Self& scope(const WithScope& sc) {
        std::string_view mod = "(global)";
        if(!sc.namespac.empty())
            mod = sc.namespac;
        return v("In namespace: `{}`", mod).o(", scope: `{}`", sc.local);
    }

    template <typename HasDocument>
    Self& doc(const HasDocument& doc) {
        if(!option.documentation)
            return *this;

        return oln("{}", doc.document);
    }

    Self& mem(const MemoryLayout& lay, bool isField) {
        using RK = enum config::MemoryLayoutRenderKind;

        if(!lay.isBitField) {
            auto kind = option.memoryLayoutRenderKind;
            const char* name[] = {"size", "align", "offset", "padding"};
            uint32_t value[] = {lay.size, lay.align, lay.offset, lay.padding};

            for(int i = 0; auto item: value) {
                v("{}: ", name[i++]);
                if(kind == RK::Both || kind == RK::Decimal)
                    v("{}", item);

                if(kind == RK::Both || kind == RK::Hexadecimal)
                    v(" (0x{:X})", item);

                if(i < 4)
                    v(" bytes, ");

                // Skip `padding` and `offset` for RecordDecl
                if(!isField && i >= (4 - 2))
                    break;
            }
        } else {
            vln("BitField: size: {} bits, offset: {} bytes + {} bits, padding: {} bits, align: {} bytes",
                lay.size,
                lay.offset >> 3,
                lay.offset & 0x7,
                lay.padding,
                lay.align);
        }

        return *this;
    }

    Self& temparms(const std::vector<Param>& params) {
        return vln("{} template parameters:", params.size())
            .iter<Param>(params, [this](const Param& p) {
                v("+ {}", p.name).o(" (default: `{}`)", p.defaultParaName).ln();
            });
    }

    Self& tags(const Fn& fn) {
        v("`{}`", fn.isConstexpr ? "constexpr" : "")
            .o(" `{}`", fn.isConsteval ? "consteval" : "")
            .o(" `{}`", fn.isInlined ? "inline" : "")
            .o(" `{}`", fn.isStatic ? "static" : "")
            .o(" `{}`", fn.isPureVirtual ? "(pure virtual)" : "")
            .o(" `{}`", fn.isOverloadedOperator ? "(overloaded operator)" : "")
            .ln();

        if(!fn.isExternallyVisible)
            vln("__Visibility: hidden__");

        if(fn.isDeprecated)
            vln("__Deprecated: {}__", fn.deprecateReason);

        return *this;
    }

    void operator() (const Record& rc) {
        // clang-format off

        // Block 1: title and namespace 
        title(rc).ln()
        .scope(rc).ln()
        .hln()

        // Block 2: optional document 
        .doc(rc)
        .hln();

        // Block 3: memory layout or template parameters
        (!rc.templateParams.empty() ? temparms(rc.templateParams) : mem(rc, /*isField=*/false)).hln();
        
        // Block 4: fields
        vif(!rc.fields.empty(), "{} fields:", rc.fields.size()).ln()
        .iter<Field>(rc.fields, [this](const Field& f) {
            // a short description in one line.
            v("+ {}", f.name).v(": `{}`", f.type.type).o(" (aka `{}`)", f.type.akaType).ln();
        })
        .hln()

        // Block 5: source code
        .vln("{}", rc.source);

        // clang-format on
    }

    void operator() (const Field& fd) {
        // clang-format off

        SymbolKind kind = SymbolKind::Field;
        // Block 1: title and namespace 
        v("{} {} `{}`", H3, kind.name(), fd.name).ln()
        .scope(fd).ln()
        .hln()

        // Block 2: type
        .v("Type: `{}`", fd.type.type).o(" (aka `{}`)", fd.type.akaType).ln()
        .mem(fd, /*isField=*/true)
        .hln()

        // Block 3: optional document 
        .doc(fd)
        .hln();

        // clang-format on
    }

    void operator() (const Enum& em) {
        // clang-format off

        // Block 1: title and namespace 
        title(em).v(" `({})`", em.implType).ln()
        .scope(em).vif(!em.isScoped, ", (unscoped)").ln()
        .hln()

        // Block 2: optional document 
        .doc(em)
        .hln()

        // Block 3: items
        .vif(!em.items.empty(), "{} items:", em.items.size()).ln()
        .iter<Enum::EnumItem>(em.items, [this](const Enum::EnumItem& it) {
            v("+ {} = `{}`", it.name, it.value).ln();
        })
        .hln()

        // Block 4: source code
        .vln("{}", em.source);

        // clang-format on
    }

    void operator() (const Fn& fn) {
        // clang-format off

        // Block 1: title and namespace 
        title(fn).ln()
        .scope(fn).ln()
        .hln()

        .tags(fn)
        .hln()

        // Block 2: template parameters
        .vif(!fn.templateParams.empty(), "{} template parameters:", fn.templateParams.size()).ln()
        .iter<Param>(fn.templateParams, [this](const Param& p) {
            v("+ {}", p.name).o(" (default: `{}`)", p.defaultParaName).ln();
        })
        .hln()

        // Block 3: return type
        .v("-> `{}`", fn.retType.type) .o(" (aka `{}`)", fn.retType.akaType).ln()
        .hln()

        // Block 4: parameters
        .vif(!fn.params.empty(), "{} parameters:", fn.params.size()).ln()
        .iter<Param>(fn.params, [this](const Param& p) {
            v("+ {}", p.name).v(": `{}`", p.type.type).o(" (aka `{}`)", p.type.akaType).o("= `{}`", p.defaultParaName).ln();
        })
        .hln()

        // Block 5: optional document 
        .doc(fn)
        .hln()
        

        // Block 6: source code
        .vln("{}", fn.source);

        // clang-format on
    }

    Self& tags(const Var& var) {
        v("`{}`", var.isConstexpr ? "constexpr" : "")
            .o(" `{}`", var.isStaticLocal ? "static" : "")
            .o(" `{}`", var.isExtern ? "extern" : "")
            .o(" `{}`", var.isFileScope ? "(file variable)" : "")
            .o(" `{}`", var.isLocal ? "(local variable)" : "")
            .ln();

        return *this;
    }

    void operator() (const Var& var) {
        // clang-format off

        // Block 1: title and namespace 
        title(var).ln()
        .scope(var).ln()
        .hln()

        // Block 2: type
        .tags(var)
        .v("Type: `{}`", var.type.type).o(" (aka `{}`)", var.type.akaType).ln()
        .vif(var.size, "size = {} bytes, align = {} bytes", var.size, var.align).ln()
        .hln()

        // Block 3: optional document 
        .doc(var)
        .hln()

        // Block 4: source code
        .vln("{}", var.source);

        // clang-format on
    }

    void operator() (const Header& ic) {
        // clang-format off
        title(ic)
        .hln()

        .vln("`{}`", ic.absPath);
        // clang-format on
    }

    void operator() (const Keyword& kw) {
        // clang-format off
        v("{} {} `{}`", H3, kw.symbol.name(), clang::tok::getTokenName(kw.tkkind))
        .hln()

        .vln("See: [{0}]({0})", kw.cpprefLink(kw.tkkind));
        // clang-format on
    }

    void operator() (const Numeric& nm) {
        bool isInteger = true;
        llvm::SmallString<64> bin;
        llvm::SmallString<32> dec;
        llvm::SmallString<32> hex;

        auto fmtter = Match{
            [&](const llvm::APInt& apint) {
                apint.toString(bin, 2, /*Signed=*/true);
                apint.toString(dec, 10, /*Signed=*/true);
                apint.toString(hex, 16, /*Signed=*/true);
            },
            [&](const llvm::APFloat& apfloat) {
                isInteger = false;
                apfloat.toString(dec, 10);
                hex.resize_for_overwrite(
                    apfloat.convertToHexString(hex.begin(),
                                               16,
                                               /*UpperCase=*/true,
                                               llvm::RoundingMode::NearestTiesToEven));
            },
        };
        std::visit(fmtter, nm.value);

        constexpr auto sv = [](llvm::StringRef str) -> std::string_view {
            return std::string_view{str.data(), str.size()};
        };

        if(isInteger) {
            // clang-format off
            v("{} {} `{}`", H3, nm.symbol.name(), sv(nm.rawText))
            .hln()
            .vln("Binary: `{}`", sv(bin))
            .vln("Decimal: `{}`", sv(dec))
            .vln("Hexadecimal: `{}`", sv(hex));
            // clang-format on
        } else {
            // clang-format off
            v("{} {} `{}`", H3, nm.symbol.name(), sv(nm.rawText))
            .hln()
            .vln("Decimal: `{}`", sv(dec))
            .vln("Hexadecimal: `{}`", sv(hex));
            // clang-format on
        }
    }

    void operator() (const Literal& lit) {
        // clang-format off
        v("{} {} `{}`", H3, lit.symbol.name(), lit.stringLiteralKindName(lit.kind))
        .hln()

        .o("User Defined Suffix: `{}`", lit.udSuffix)
        .hln()

        .vln("size: {} bytes", lit.content.size() + 1) // null-terminated
        .hln()

        .vln("{}", lit.content);
        // clang-format on
    }

    void operator() (const Expression& expr) {
        // clang-format off
        v("{} Expression `{}`", H3, expr.name)
        .hln()

        .v("type: `{}`", expr.type.type).o("  (aka `{}`)", expr.type.akaType).ln()
        .hln()

        .vln("{}", expr.source);
        // clang-format on
    }

    void operator() (const Empty&) {
        // Show nothing in release mode to avoid crash.
#ifndef NDEBUG
        llvm_unreachable("Empty just used to defualt-construct a HoverInfo");
#endif
    }

    /// Render the hover information to markdown text.
    static std::string print(const HoverInfo& hover, config::HoverOption option) {
        constexpr size_t kMaxBufferSize = 512;

        MarkdownPrinter pv{.option = option};
        /// Reserve at most 512 bytes for the buffer.
        pv.buffer.reserve(std::min(std::bit_ceil(hover.estimated_size()), kMaxBufferSize));

        std::visit(pv, hover);

        while(pv.buffer.ends_with(HLine))
            pv.buffer.resize(pv.buffer.size() - HLine.size());

        // Keep at most one \n in the tail.
        while(pv.buffer.ends_with("\n\n"))
            pv.buffer.pop_back();

        pv.buffer.shrink_to_fit();
        return std::move(pv.buffer);
    }
};

}  // namespace

}  // namespace clice

namespace clice::feature::hover {

namespace {

Result toMarkdown(const HoverInfo& hover, const config::HoverOption& option) {
    return {.markdown = MarkdownPrinter::print(hover, option)};
}

std::optional<HoverInfo>
    hover(uint32_t line, uint32_t col, ASTInfo& AST, const config::HoverOption& option) {

    auto srcLoc = AST.srcMgr().translateLineCol(AST.mainFileID(), line, col);
    if(srcLoc.isInvalid())
        return std::nullopt;

    auto tokens = clang::syntax::spelledTokensTouching(srcLoc, AST.tokBuf());
    if(tokens.empty())
        return std::nullopt;

    // Check if the position is in a include directive.
    llvm::ArrayRef<Include> includes = AST.directives()[AST.mainFileID()].includes;
    if(auto hit = hit::header(includes, AST, line))
        return hit;

    auto candidates = pickBestToken(tokens);
    for(const auto& token: candidates) {
        if(auto hit = hit::detect(token, AST, option))
            return hit;
    }

    return std::nullopt;
}

}  // namespace

Result hover(const clang::Decl* decl, const config::HoverOption& option) {
    return toMarkdown(DeclHoverBuilder::build(decl, option), option);
}

std::optional<Result> hover(const proto::HoverParams& param,
                            ASTInfo& AST,
                            const config::HoverOption& option) {
    // Convert 0-0 based lsp position to clang 1-1 based loction.
    return hover(param.position.line + 1, param.position.character + 1, AST, option)
        .transform([&option](HoverInfo&& hv) { return toMarkdown(hv, option); });
}

proto::MarkupContent toLspType(Result hover) {
    return {.value = std::move(hover.markdown)};
}

}  // namespace clice::feature::hover
