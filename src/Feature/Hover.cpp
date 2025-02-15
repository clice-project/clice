
#include "Feature/Hover.h"

#include <clang/AST/DeclVisitor.h>

#include <variant>
#include <format>

template <class... Ts>
struct Match : Ts... {
    using Ts::operator()...;
};

using LocalStr = std::pmr::string;

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

struct Field : WithDocument, MemoryLayout {

    PrettyType type;

    std::string name;

    std::string scope;

    clang::AccessSpecifier access;

    size_t estimated_size() const {
        return WithDocument::estimated_size() + sizeof(MemoryLayout) + type.estimated_size() +
               name.size() + scope.size();
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

    // Non empty if isDeprecated is true.
    std::string deprecateReason;

    size_t estimated_size() const {
        return HoverBase::estimated_size() + WithDocument::estimated_size() +
               WithScope::estimated_size() + type.estimated_size();
    }
};

struct Empty : std::monostate {
    size_t estimated_size() const {
        return 0;
    }
};

/// Use variant to store different types of hover information to reduce the memory usage.
struct HoverInfo : public std::variant<Empty, Namespace, Record, Enum, Fn, Var> {
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

    void recFillScope(const clang::DeclContext* DC, WithScope& scope) {
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
            scope.local += FD->getName(), scope.local += "::";
        }
    }

    void fillScope(const clang::DeclContext* DC, WithScope& scope) {
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
        ns.name = ND->getName();

        recFillScope(ND->getDeclContext(), ns);

        ns.source = "<TODO: source code>";
        hover.emplace<Namespace>(std::move(ns));
    }

    void fillMemoryLayout(const clang::RecordDecl* FD, MemoryLayout& lay) {
        clang::ASTContext& ctx = FD->getASTContext();
        clang::QualType QT = ctx.getRecordType(FD);

        lay.align = ctx.getTypeAlignInChars(QT).getQuantity();
        lay.size = ctx.getTypeSizeInChars(QT).getQuantity();
    }

    void VisitRecordDecl(const clang::RecordDecl* RD) {
        Record rc;

        // For class (partial) template specialization, use existing one to reuse its template
        // params.
        if(std::holds_alternative<Record>(hover)) {
            rc = std::get<Record>(std::move(hover));
            hover.emplace<Empty>();
        }

        rc.symbol = RD->isStruct() ? SymbolKind::Struct : SymbolKind::Class;
        rc.name = RD->getNameAsString();

        fillMemoryLayout(RD, rc);
        fillScope(RD->getDeclContext(), rc);

        rc.source = "<TODO: source code>";
        rc.document = "<TODO: document>";
        hover.emplace<Record>(std::move(rc));

        for(auto FD: RD->fields()) {
            VisitFieldDecl(FD);
        }
    }

    void VisitClassTemplateDecl(const clang::ClassTemplateDecl* TD) {
        Record rc;

        for(const auto& tpara: *TD->getTemplateParameters()) {
            Param p;
            p.name = tpara->getName();
            rc.templateParams.push_back(std::move(p));
        }
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

    void VisitFieldDecl(const clang::FieldDecl* FD) {
        assert(std::holds_alternative<Record>(hover) && "Must be a Record hover");

        Field field;
        field.access = FD->getAccess();
        field.name = FD->getNameAsString();

        auto RD = llvm::dyn_cast<clang::RecordDecl>(FD->getDeclContext());
        field.scope = RD->isAnonymousStructOrUnion() ? "(anonymous)" : RD->getName();
        fillMemoryLayout(FD, RD, field);

        auto ty = FD->getType();
        auto canonicalTy = ty.getCanonicalType();  // ignore typedef

        if(ty == canonicalTy) {
            field.type.type = ty.getAsString();
        } else {
            field.type.type = ty.getAsString();
            field.type.akaType = canonicalTy.getAsString();
        }

        auto& record = std::get<Record>(hover);
        record.fields.push_back(std::move(field));
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

    void VisitFunctionDecl(const clang::FunctionDecl* FD) {
        Fn fn;

        /// For function template, use existing one to reuse its template parameters.
        if(std::holds_alternative<Fn>(hover)) {
            fn = std::get<Fn>(std::move(hover));
            hover.emplace<Empty>();
        }

        if(fn.symbol != SymbolKind::Method && fn.symbol != SymbolKind::Operator) {
            fn.symbol = SymbolKind::Function;
        }

        fn.name = FD->getName();
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

        hover.emplace<Fn>(std::move(fn));
    }

    void VisitFunctionTemplateDecl(const clang::FunctionTemplateDecl* TD) {
        Fn fn;

        for(auto tpara: *TD->getTemplateParameters()) {
            Param p;
            p.name = tpara->getName();
            fn.templateParams.push_back(std::move(p));
        }

        hover.emplace<Fn>(std::move(fn));
        VisitFunctionDecl(TD->getAsFunction());
    }

    void VisitCXXMethodDecl(const clang::CXXMethodDecl* MD) {
        Fn fn;
        fn.symbol = SymbolKind::Method;
        hover.emplace<Fn>(std::move(fn));

        VisitFunctionDecl(MD);
    }

    void VisitVarDecl(const clang::VarDecl* VD) {
        Var var;
        var.symbol = SymbolKind::Variable;
        var.name = VD->getName();
        var.isConstexpr = VD->isConstexpr();
        var.isFileScope = VD->isFileVarDecl();
        var.isLocal = VD->isLocalVarDecl();
        var.isStaticLocal = VD->isStaticLocal();
        var.isExtern = VD->isLocalExternDecl();
        var.isDeprecated = VD->isDeprecated(&var.deprecateReason);

        fillScope(VD->getDeclContext(), var);

        auto qty = VD->getType();
        var.type.type = qty.getAsString();
        if(auto cqty = qty.getCanonicalType().getAsString(); cqty != var.type.type) {
            var.type.akaType = std::move(cqty);
        }

        auto& ctx = VD->getASTContext();
        var.size = ctx.getTypeSizeInChars(qty).getQuantity();
        var.align = ctx.getTypeAlignInChars(qty).getQuantity();

        var.document = "<TODO: document>";
        var.source = "<TODO: source code>";
        hover.emplace<Var>(std::move(var));
    }
};

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
    Self& doc(const HasDocument& bs) {
        if(!option.documentation)
            return *this;

        return oln("{}", bs.document);
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

    void operator() (const Enum& enm) {
        // clang-format off

        // Block 1: title and namespace 
        title(enm).v(" `({})`", enm.implType).ln()
        .scope(enm).vif(!enm.isScoped, ", (unscoped)").ln()
        .hln()

        // Block 2: optional document 
        .doc(enm)
        .hln()

        // Block 3: items
        .vif(!enm.items.empty(), "{} items:", enm.items.size()).ln()
        .iter<Enum::EnumItem>(enm.items, [this](const Enum::EnumItem& it) {
            v("+ {} = `{}`", it.name, it.value).ln();
        })
        .hln()

        // Block 4: source code
        .vln("{}", enm.source);

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
        .vln("size = {} bytes, align = {} bytes", var.size, var.align)
        .hln()

        // Block 3: optional document 
        .doc(var)
        .hln()

        // Block 4: source code
        .vln("{}", var.source);

        // clang-format on
    }

    void operator() (const Empty&) {
        buffer = "<unimplemented>";
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

Result hover(const clang::Decl* decl, const config::HoverOption& option) {
    assert(decl && "Must be non-null pointer");

    DeclHoverBuilder builder{.option = option};
    builder.Visit(decl);
    return {.markdown = MarkdownPrinter::print(builder.hover, option)};
}

/// Compute inlay hints for MainfileID in given param and config.
Result hover(proto::HoverParams param,
             ASTInfo& info,
             const SourceConverter& converter,
             const config::HoverOption& option) {
    assert(false && "Not implemented yet");
    return {};
}

proto::MarkupContent toLspType(Result hover) {
    proto::MarkupContent markup;
    markup.value = std::move(hover.markdown);
    return markup;
}

}  // namespace clice::feature::hover
