# 谁在摧毁 header context
header context 的基本假设是，如果一个头文件在不同的包含位置产生完全相同的 AST。那么这两个位置是同一个 header context。

然而这个假设并不可靠，首先 clang 的 AST 并非彻底的 Tree 形式，而是又想无换图。当模板实例化出现的时候会把它加到父节点的 AST 中，这说明对比 AST 相同是不可靠的。就算这个是可靠的，也不存在高效比较 AST 相同的办法。而且我们实际上需要进行的是跨 AST 的比较，考虑到内存占用和并行度等问题，并不能通过 ASTImport 来进行这种比较，我们需要采用一种实际可行的算法来判断给定的两个头文件位置是否是相同的 header context。

项目中绝大多数文件都是 self contained 的，在不同源文件中应该产生相同结果。

# 初步解决方案

最初的想法是比较直接的，我们对给定头文件运行所有的 readonly LSP 的 feature 和 index，产生二进制索引文件。如果它们完全的 hash 相同则是相同的 header context。这个依赖于一个假设，即使存在两个二进制相同的但是代表不同 header context 的情况，用户也是无法分辨的。

基于这个假设，我编写了最初的索引器，并在 clice 自身上进行了一些尝试。果然发现了一些意料之外的情况，主要的问题就是，上述的假设太容易被打破了。存在一些“用户期望的相同上下文”，但是索引结果却不同导致被识别为不同的 header context。

具体有哪些呢？一一记录如下

## 模板 lazy instantiation 导致的不一致（跳转特性）

```cpp
/// header
template<typename T, typename U>
struct X {};

template<typename T>
struct X<T, int> {};

using Z = X<int, int>

/// source
/// Z z
```

注释掉 `Z z` 与否，会导致 `X<int, int>` 中 X 跳转结果不一致。未注释的跳转结果是偏特化，注释了则是主模板。这是因为 C++ 中的模板实例化是 lazy 的，如果不需要完整类型的话并不会触发类模板的实例化。不实例化就不会选择具体的模板，于是此时 clangd 进行 fallback 给你返回了主模板。

## auto 函数导致函数签名不一致（hover, inlay hint）

```cpp
/// header
auto foo();

/// source
auto foo() {
    return 1;
}
```

hover 中展示的函数签名会根据 foo 有没有定义而不同，有定义的话就是 `int foo()`，否则就是 `auto foo()`。返回值的 inlay hint 同理，没有具体类型就没有，否则就有。 

## document 出现在定义导致的不一致（hover, completion）

```cpp
/// header
auto foo();

/// source
/// function foo
auto foo() {
    return 1;
}
```

只有看见定义的时候，才能看见 document。

## 函数签名不一致导致的 source code 不同

```cpp
/// header
void foo() [[deprecated("")]];

/// source
void foo() {}
```

hover 信息中展示的函数签名不同

## 伪实例化器或者模板实例化导致的跳转额外信息不同

由于伪实例化器在整个 AST 都 parse 完了之后再进行工作的，如果看到的前面的声明不同，比如某个偏特化没看到，那么求值的结果当然不同。

```cpp
/// header
template <typename T>
void foo() {
    T::foo();
}

/// source
struct X {
    void foo():
};

int main() {
    foo<X>();
}
```

我们希望能从模板代码中，跳到实例化的具体类型。显然会因为在当前编译单元有没有实例化而导致跳转结果不同

## 根据宏的实例化来提供信息

```cpp
/// header
#define def(type) type x = 1;

/// source
def(int)
```

我们也想能在查看宏代码的时候，选择某个具体的实例化来查看信息，包括对原本的宏里面的内容做高亮。显然这也会因为在编译单元中有没有宏的实例化而不同。

## 总结

目前在尝试实现 header context 的索引的时候就遇到了上面这些问题。它们都会导致“本来预期是完全相同的代码”产生一些细微的不一致，从而产生多份索引。有什么好的处理办法能解决这么一长串的问题吗？如果无法解决，那么 header context 必然是不完整，不可用的。索引系统的实现也难以继续往下推进。除此之外，之后我们可能还要考虑储存 diagnostic（可选），这个会让上述的问题变得更加严重。以及可能会有一些用户层面的 ODR 违反也会导致类似的问题变得更严重。

可以发现绝大多数问题都是由于 C++ 傻逼的前后分离声明导致的。一种很直接明了的解决办法是把头文件的多份索引合并起来，并设置一些优先级。像文档和声明这种优先级高的就可以覆盖优先级低的东西，初步打算采用这种方案来进行解决。

合并的问题有哪些呢？
- 如何进行合并？如何更新结果？增加，修改和减少上下文？
- 在一个已经合并的结果中，如何区分来自不同上下文的结果？
- 如何尽可能小的缩小内存占用量？考虑三种索引，SymbolIndex，FeatureIndex，CompletionIndex，如果保证三种均能在这种 pattern 下面工作？

# 索引设计

- SymbolIndex，记录 Symbol 间的依赖关系等信息，用于支持 go to definition 这样的查询请求（精确查找）
- FeatureIndex，只读 LSP 请求结果的缓存
- CompletionIndex，代码补全缓存，对文档的 qualifier name 进行倒排索引，支持符号的模糊查找。以及未来可能会加入的自由函数补全不能，也就是`FILE* p`，可以补全`p->close`成，`close(p)`。注意这种情况下我们不考虑第一个参数是模板参数的情况。只考虑具体的类型作为参数的情况，显然想要实现也得建立一个索引，也就是函数的第一个类型到整个函数的索引。

待定的索引信息：
- 模板实例化的完整信息，对于给定的模板实例化，索引其 body，提供模板实例化的索引结果，然后储存起来。给用户一个选项，运行选择 T 为某种特定类型的时候，我们创建一个（只读的）临时文件来展示实例化结果。然后允许用户在临时文件上查看代码补全，等只读 LSP 请求。
- 宏实例化的完整信息，同上，只不过实例化变成了宏。创建一个临时文件，展示当前宏的实例化结果。用户可以轻松的在临时文件上运行只读的 LSP 请求。

下面这两个需求并不着急，我们可以先把它们限制在 writeable 的文件上，然后用内存中的 AST 返回结果。之后再进一步讨论看看是否能把它们索引在磁盘上（并且支持查询）。

这就是目前索引器的主要问题，计划在四月底完成设计，五月底完成整个实现。

好的现在我有一些想法了，

对于给定的头文件索引，首先我们有一个 

```cpp
struct MergedIndex {
    std::uint32 maxID;
    std::vector<HeaderContext> contexts;
}
```

每个 HeaderContext 有一个 ID 成员记录它的 ID，所有具有相同上下文的 HeaderContext 具有相同的 ID。

然后对于每个索引，以 SemanticToken 为例，我们会存一个等价的 indices。

`std::vector<std::uint32_t>` 作为它的上下文数组。每个 uint32 的含义随着 maxID 的不同而不同。如果 maxID 小于 31，那么表示这个 uint32 是一个 bitmap。每一位代表对应 ID 的 header context 是否拥有这个元素。

这样我们可以轻松的区分给定元素在给定的 header context 下是否存在。如果要筛选出给定 header context 的所有元素也很轻松。

问题来了，这样就解决前面说的上下文问题了吗？

是的，解决了。如何解决的呢？关键点就是我们可以采用自定义的算法来合并索引了。那么对于特殊的 AST 节点产生的索引我们可以采用一些标记。比如对于模板实例化产生的索引，我们不认为它会造成 ID 的增加，我们直接在 ID 中储存它对应的 header context 的 index，采用 uint32 的最高位作为标记。

