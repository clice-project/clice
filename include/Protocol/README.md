This file dictionary mainly describes the [Language Server Protocol Specification](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/). Every interface in the protocol is defined as corresponding struct. 

For example, [Position](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#position) has the following definition in the protocol:

```typescript
interface Position {
	line: uinteger;
	character: uinteger;
}
```

then the corresponding representation in C++ is:

```cpp
struct Position {
    uinteger line;
    uinteger character;
};
```

We use template meta programming to reflect the protocol type so that the serialization and deserialization can be done automatically. The trick is only suitable for type which is aggregate and default constructible and does not have base classes. But there are many types in the protocol which are defined through inheritance. For example, [DeclarationParams](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#declarationParams) is defined as:

```typescript
export interface DeclarationParams extends TextDocumentPositionParams,
	WorkDoneProgressParams, PartialResultParams {
}
```

We use `Combine` to resolve the problem. `Combine` is defined as:

```cpp
template<typename... Ts>
struct Combine : Ts... {};
```

Then `DeclarationParams` can be defined as:

```cpp
using DeclarationParams = Combine<
    TextDocumentPositionParams, 
    WorkDoneProgressParams, 
    PartialResultParams
>;
```

Compared to direct inheritance, this way allows us to get the base class types of `DeclarationParams`. If the interface also has data members, you should define a struct to hold the data members separately and add it to `Combine` list.

For example, we have [ReferenceParams](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#referenceParams) definition as follow:

```typescript
export interface ReferenceParams extends TextDocumentPositionParams,
	WorkDoneProgressParams, PartialResultParams {
	context: ReferenceContext;
}
```

Then we can define `ReferenceParams` as:

```cpp
struct ReferenceParamsBody {
    ReferenceContext context;
};

using ReferenceParams = Combine<
    TextDocumentPositionParams, 
    WorkDoneProgressParams, 
    PartialResultParams,
    ReferenceParamsBody
>;
```

For number enum in TypeScript, it can define as corresponding `enum` in C++ directly, for string enum, we define it as `struct` with `static` members.

```typescript
export namespace MarkupKind {
	export const PlainText: 'plaintext' = 'plaintext';
	export const Markdown: 'markdown' = 'markdown';
}
export type MarkupKind = 'plaintext' | 'markdown';
```

```cpp
struct MarkupKind {
    std::string_view m_value;

    constexpr MarkupKind(std::string_view value) : m_value(value) {}

    constexpr inline static std::string_view PlainText = "plaintext";
    constexpr inline static std::string_view Markdown = "markdown";
};
```


