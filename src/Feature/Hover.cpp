
#include "Feature/Hover.h"

#include <clang/AST/DeclVisitor.h>

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
};

struct WithScope {
    /// For class/struct/enum in some namespace, like "NamespaceA::NamespaceA::", maybe empty.
    std::string namespac;

    /// For nested class, like "StructA::StructB::", maybe empty.
    std::string local;
};

struct Namespace : HoverBase, WithScope {};

/// Memory layout information of a class/struct/field.
struct MemoryLayout {
    bool isBitField = false;
    uint8_t padding;  // bits if isBitField is true, otherwise in bytes
    uint16_t size;    // bits if isBitField is true, otherwise in bytes
    uint16_t offset;  // bits if isBitField is true, otherwise in bytes
    uint16_t align;   // always in bytes
};

struct PrettyType {
    /// The type of the item in a more human-readable format.
    std::string type;

    /// Desugared type of the item. （aka `SomeDesugaredType`)
    std::string akaType;
};

struct Field {
    PrettyType type;

    std::string name;

    clang::AccessSpecifier access;
};

struct Record : HoverBase, WithScope, MemoryLayout {
    std::vector<Field> fields;
};

struct Enum : HoverBase, WithScope {
    // std::vector<std::string> fields;
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
};

struct FuncProto : HoverBase, WithScope {
    PrettyType retType;

    std::vector<Param> params;

    std::vector<Param> templateParams;
};

/// Use variant to store different types of hover information to reduce the memory usuage.
struct HoverInfo : public std::variant<Namespace, Record, Enum, FuncProto> {
    /// Return the estimated size of the hover information in bytes.
    size_t estimated_size() const {
        auto counter = Match{
            [](const Record& record) -> size_t { return record.fields.size() * sizeof(Field); },
            [](const Enum& enm) -> size_t { return sizeof(Enum); },
            [](const FuncProto& func) -> size_t { return func.params.size() * sizeof(Param); },
            [](const Namespace& space) -> size_t { return sizeof(Param); },
        };
        return std::visit(counter, *this);
    }
};

struct MarkdownPrinter {

    std::string buffer;

    // std::string memoryLayout(const MemoryLayout& layout, config::MemoryLayoutRenderKind kind) {}

    void operator() (const Namespace& space) {}

    void operator() (const Record& record) {}

    void operator() (const Enum& enm) {}

    void operator() (const FuncProto& func) {}

    /// Render the hover information to markdown text.
    static std::string print(const HoverInfo& hover) {
        constexpr size_t kMaxBufferSize = 512;

        MarkdownPrinter printer;
        /// Reserve at most 512 bytes for the buffer.
        printer.buffer.reserve(std::min(std::bit_ceil(hover.estimated_size()), kMaxBufferSize));

        std::visit(printer, hover);

        if(printer.buffer.size() > kMaxBufferSize)
            printer.buffer.shrink_to_fit();
        return std::move(printer.buffer);
    }
};

struct DeclHoverBuilder : public clang::ConstDeclVisitor<DeclHoverBuilder, void> {
    using Base = clang::ConstDeclVisitor<DeclHoverBuilder, void>;

    const config::HoverOption option;

    HoverInfo hover;

    void VisitNamespaceDecl(const clang::NamespaceDecl* ND) {}

    void VisitRecordDecl(const clang::RecordDecl* RD) {}

    void VisitFieldDecl(const clang::FieldDecl* FD) {
        assert(std::holds_alternative<Record>(hover) && "Must be a Record hover");
    }

    void VisitEnumDecl(const clang::EnumDecl* ED) {}

    void VisitFunctionDecl(const clang::FunctionDecl* FD) {}
};

}  // namespace

}  // namespace clice

namespace clice::feature::hover {

HoverResult hover(const clang::Decl* decl, const config::HoverOption& option) {
    DeclHoverBuilder builder{.option = option};
    builder.Visit(decl);
    return {.markdown = MarkdownPrinter::print(builder.hover)};
}

proto::MarkupContent toLspType(HoverResult hover) {
    proto::MarkupContent markup;
    markup.value = std::move(hover.markdown);
    return markup;
}

}  // namespace clice::feature::hover
