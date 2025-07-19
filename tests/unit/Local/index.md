# clice 的进度概览

FAQ: 什么时候能用上？

clice 的代码可以看成两个主要部分：
1. 核心 Server 的部分，负责接收 client 的请求，并调度各种编译任务的执行，调度索引查询，监听文件更新，响应 client 等等
2. 各个 Feature 的实现，基于 AST 或者其它的 clang API 收集信息，并组织成 LSP 需要的格式

原则上来说，只要 1 的部分可以用了，那么 clice 的就可用了，即使各个 feature 没实现完也没关系，可以暂时只使用实现好的 feature。尽管如此，但是 Server 部分的编写还是有些麻烦的，具体麻烦在哪里呢？其实就是语言服务器要调度的任务太多了，各个任务之间可能还有较为复杂的依赖关系。如果在早期不对服务器进行充分的设计，以尽可能考虑较多的情况，那么 Server 部分后续想要添加代码就会非常困难。所以这部分的代码一直没有太好的完成。

# Server 

为了进一步阐明究竟复杂在哪里，下面列出 Server 的一些基本要求

## 启动的时候

- Server 在启动的时候需要读取 `clice.toml`，这是 clice 的配置文件，并在收到 client 的 `onInitialize` 请求之后对配置文件里面的诸如 `workspace` 这样的变量进行替换，除非用户在命令行显式指定该变量。
- Server 需要监听配置文件中设置的所有 CDB 文件，并在 CDB 文件进行更新的时候做出一些反应。比如检查哪些文件被增加，修改，或者编译命令改变以需要重新进行编译的。还需要扫描所有的 module interface unit 来建立一个 module name 到文件路径的映射。

## 涉及到文件操作

- 在收到 `didOpen` 通知的时候，也就是打开一个文件的时候，Server 需要做出一些抉择。根据用户在配置文件中指定的 Rule 对该文件进行处理，如果文件是 `readonly`，那么判断该文件是否有现成的索引（或者需要更新），如果没有则立即索引该文件（不构建 PCH）否则什么都不做。如果是 `writable`，那么判断该文件是否有现成的 PCH（或者需要更新），没有就立刻为它构建 PCH。如果是 `auto`，先把它当成 `readonly` 的，直到用户第一次编辑它，这时候再把它当成 `writable` 的进行处理，具体的处理方式同上。
- 在收到 `didChange` 通知的时候，首先我们在 LSP 请求里面设置的是增量更新模式，client 会发送一组 range 来表示更新的文本。我们需要判断用户是否正在编辑 preamble，如果正在编辑 preamble 则什么都不做，等到 preamble 编辑结束的时候，就更新 PCH。如果不在编辑 preamble，就构建一个新的 AST 即可。
- 在收到 `didClose` 通知的时候怎么处理呢？对于所有打开的文件，我倾向于用一个 LRU 容易来储存它们，并且允许用户设置一个最大数量（或者内存限制），超过这个数量的文件的 AST 会被清理。因为 AST 是非常占用内存的，对于每个打开的文件，我们如果为他们都保留一个 AST，那么 clice 的内存占用就会非常高。所以对于很久不用的文件的 AST，可以把它删了。反正已经构建好 PCH 了，再去构建 AST 是很快的（PCH 可以存在磁盘上，但是 AST 不能）。
- 在收到 `didSave` 的时候，对于源文件没啥好做的，对于头文件，要更新包含了它的所有源文件（重新索引）

对于文件的 `create`, `rename`, `delete`, 目前我还没有什么想法，这里可以进一步讨论。比如 `rename` 一个头文件的话，自动更新所有 include 它的源文件。（不过根据我的实际体验，这样做可能会导致每次创建删除或者重命名时间过长，clion 就有这个功能）。

注意，AST 的遍历并非线程安全的，如果有多个任务需要遍历同一个 AST，它们需要排队执行。

## 具体到请求

- `CodeCompletion`（提供代码补全信息） 和 `SignatureHelp`（用于选择重载函数），这两个请求的处理方式是类似的，都是单独运行一次 clang（打开 CodeCompletion 模式），写一个 Consumer 来获取结果。Consumer 相关的逻辑在 feature 中进行处理的，对于 Server 来说只需要在线程池中调度该任务执行然后返回结果就行了。此任务依赖于 PCH 的构建完成（未来可能还会支持从索引中查询一些信息以添加额外的代码补全信息）。
- `SemanticToken`，`InlayHint`，`FoldingRange`，`DocumentLink`，`DocumentSymbol`，`Hover` 这六个请求是典型的 readonly 的 LSP 请求，对于 readonly 的文件，我们直接读取 FeatureIndex 来进行响应（不需要 AST）。对于 writeable 的文件，则在它们上面运行对应的 feature 函数即时返回结果。
- 对于 `CodeAction` 和 `Rename` 这种请求，是典型的 writeable 的请求，只能在非 readonly 文件上面触发，CodeAction 的实现需要使用 SelectionTree 判断用户选中的 AST 的范围（由于需要 AST，所以只能是 writeable 的文件），然后进行一些处理。
- 对于 `GotoDefinition`，`GotoDeclaration`，`GotoTypeDefinition`，`GotoImplementation`，`FindReferences`，`OutgoingCalls`，`IncomingCalls`，`SuperTypes`，`SubTypes` 这些请求全都只需要通过查询索引实现，但是具体对索引的查询方式有些区别，这个放到后面专门的索引小节里面再说了。
- 对于 `Formatting` 和 `RangeFormatting` 这两个直接调用 clang 的 API 进行就行了，这个 format 是基于 token 的不需要 AST，也不需要 PCH，和其它的特性是完全分离的，不需要任何额外的处理（其实和 clang format 用的是相同的函数）。

## 关于索引

对于单片索引来说，如何查询某个符号的信息之前已经说过了，接下来具体说说如何查询所有的索引文件。以及对于 `Indexer` 的需求有哪些

首先最基础的需求，我们需要判断一个文件是否需要重新索引，以及在 clice 关闭之后下次再打开，原来的索引是否能重新复用。于是我们需要储存一些索引文件的元信息，用来判断是否需要重新索引。以及在服务器关闭的时候把这些信息存到磁盘上，下次打开的时候再从磁盘上进行加载。

不仅如此，我们还需要能够根据源文件的路径获取到对应的索引文件的路径，以方便我们读取索引文件并且进行查询。另外由于我们要支持 header context 这个特性，还需要查询头文件和源文件之间的包含关系（clangd 有一项功能是在头文件和源文件之间进行切换，只要我们能记录头文件和源文件之间的包含关系，想记录这个信息是轻而易举的）。

总结一下，有三个需求
- 储存元信息用于判断文件是否需要重新索引
- 能够查询到源文件对应的索引文件路径
- 查询头文件和源文件之间的上下文关系

目前 clice 中的参考实现如下

```cpp
struct TranslationUnit;

struct HeaderIndex {
    /// The index file path(not include suffix, e.g. `.sidx` and `.fidx`).
    std::string path;

    /// The hash of the symbol index.
    llvm::XXH128_hash_t symbolHash;

    /// The hash of the feature index.
    llvm::XXH128_hash_t featureHash;
};

struct Context {
    /// The index of header context in indices.
    uint32_t index = -1;

    /// The location index in corresponding tu's
    /// all include locations.
    uint32_t include = -1;
};

struct IncludeLocation {
    /// The location of the include directive.
    uint32_t line = -1;

    /// The index of the file that includes this header.
    uint32_t include = -1;

    /// The file name of the header in the string pool. Beacuse
    /// a header may be included by multiple files, so we use
    /// a string pool to cache the file name to reduce the memory
    /// usage.
    uint32_t file = -1;
};

struct Header;

struct TranslationUnit {
    /// The source file path.
    std::string srcPath;

    /// The index file path(not include suffix, e.g. `.sidx` and `.fidx`).
    std::string indexPath;

    /// All headers included by this translation unit.
    llvm::DenseSet<Header*> headers;

    /// The time when this translation unit is indexed. Used to determine
    /// whether the index file is outdated.
    std::chrono::milliseconds mtime;

    /// All include locations introduced by this translation unit.
    /// Note that if a file has guard macro or pragma once, we will
    /// record it at most once.
    std::vector<IncludeLocation> locations;

    /// The version of the translation unit.
    uint32_t version = 0;
};

struct HeaderContext {
    TranslationUnit* tu = nullptr;

    Context context;

    bool valid() {
        return tu != nullptr;
    }
};

struct Header {
    /// The path of the header file.
    std::string srcPath;

    /// The active header context.
    HeaderContext active;

    /// All indices of the header.
    std::vector<HeaderIndex> indices;

    /// All header contexts of this header.
    llvm::DenseMap<TranslationUnit*, std::vector<Context>> contexts;

    /// Given a translation unit and a include location, return its
    /// its corresponding index.
    std::optional<uint32_t> getIndex(TranslationUnit* tu, uint32_t include);
};

struct IncludeGraph {
    llvm::StringMap<Header*> headers;
    llvm::StringMap<TranslationUnit*> tus;
    std::vector<std::string> pathPool;
    llvm::StringMap<std::uint32_t> pathIndices;
}; 
```

简单来说有两个 `StringMap`，分别存了所有的 `Header` 和 `TranslationUnit`，并且 `Header` 和 `TranslationUnit` 互相存储了对方的引用以方便进行查询。

- 对于需求 `1`，在 `TranslationUnit` 中有一个 `mtime` 字段，储存了上次索引该文件的时候。只需要对比源文件本身以及它依赖的头文件的保存时间是否比这个新就可以了，如果是的话就需要 update 否则不需要。

- 对于需求 `2`，`StringMap` 本身就是源文件路径到元信息的映射，找到对应的 `Header` 或者 `TranslationUnit` 然后从里面查询索引文件路径就行了。

- 包含图关系也是轻松的进行记录。一个关键的问题是在上述的代码里面是如何记录 header context 的呢？首先 C++ 的基本编译单元是一个源文件，而一个源文件有一组有意义的 include（没有因为头文件保护等原因被跳过）。源文件所有的 `IncludeLocation` 被存在了 `TranslationUnit` 中。注意一个头文件可能在一个源文件中有多个上下文，是根据 include location 来决定的。所以 源文件路径 + `IncludeLocation` 才能真的确定一个 header context。在上述的代码里面用一个 `uint32_t` 表示 `IncludeLocation`，如果用户想要查询这个 `IncludeLocation` 具体的 include chian，也就是整条 chain 上的包含信息，可以之后拿着这个 `uint32_t` 和 `TranslationUnit` 进行 lazy 的 resolve。具体的内容在下面的 HeaderContext 小节叙述。

上面的设计基本可以 cover clice 对 `Indexer` 需要储存的信息的要求了，应该不需要进行太大的需求。


## 关于 HeaderContext 

在这里详细叙述一下 clice 对 header context 这个特性计划提供的支持，以及如何支持。从索引到 PCH 构建均会涉及。

### 如何定义 HeaderContext

目前的索引器会问一个 TU 中的所有文件发出结果，在一个 TU 中这可以用 clang 的 `FileID` 进行区分。每次 `include` 一个文件 clang 都会为它分配一个新的 `FileID`。而每一个 `FileID` 都可以代表一个头文件在当前编译单元的 header context。请牢记，一个头文件可能在同一个源文件中有多个 header context。如果需要跨编译单元进行表示，那么上述的 `FileID` 不可以使用，转而使用我们上述索引过程中提到的 文件路径 + `IncludeLocation` 进行区分即可。

### 如何正确提示文件

现在假设用户打开了一个非自包含文件，clice 改如何处理它来使它能表现得像一个正常的文件呢？

首先如果该文件是 readonly 的，那么我们可以直接尝试从 `FeatureIndex` 中读取结果，为它提供 SemanticToken 等 feature 的支持。如果它有多个上下文，只需要切换读取那个上下文对应的索引文件就行了，很轻松就能支持。分片索引天然支持 header context。这种情况是比较好处理的。

那么假设这个文件是 writable 的，该怎么处理？为了加快编译速度，我们需要为该文件构建 PCH，然后构建 AST，怎么做呢？

考虑如下的简单案例

假设我们有正在编辑 `test.h`，它的主文件是 `test.cpp`，具体的包含图如下所示

```cpp
/// test.h
...

/// test2.h
#include <vector>
#include "test.h"

/// test3.h
#include <string>

void foo();

struct Bar {
    int x;
};

#include "test2.h"

/// test.cpp
#include <tuple>
#include "test3.h"
```

怎么让 `test.h` 能正常编译呢？其实原理并不复杂，我们可以对 `test.cpp` 进行预处理，然后递归展开头文件，直到展开到 `test.h`（其实不执行预处理也行，就是粘贴复制文件内容）。

上面的按照下面这个步骤一步步展开，先展开 `test3.h`

```cpp
#include <tuple>
#include <string>

void foo();

struct Bar {
    int x;
};

#include "test2.h"
```

继续展开 `test2.h`

```cpp
#include <tuple>
#include <string>

void foo();

struct Bar {
    int x;
};

#include <vector>
#include "test.h"
```

这里就获取到了 `test.h` 前面所有的代码，如果能通过某种不影响 `test.h` 自身代码的方式，把上面的代码加到它的前面去就好了，这样就可以正常编译了。那么能做到吗？可以的，clang 有一个命令叫 `-Xinclude` 可以隐式的 include 一个文件，而不改变文件主体的内容。我们只需要通过该命令把手动展开得到的代码添加到 `test.h` 前面就好了，然后照常为它构建 PCH。这样就完成了对非自包含头文件的编辑支持！

具体如何展开头文件呢？其实只要有 include 链就能展开了（里面有 include 指令的 line 位置）。可以从之前的索引信息里面进行查询，也可以单独运行一次预处理（预处理比编译快非常多非常多，不用担心时间问题）。


## Design

这里对 Server 的早期设计进行一些总结，作为对未来设计的参考

- `Scheduler` 这个类理论上来说负责调度所有线程池中任务的执行，并储存一些相关的状态记录磁盘上的 PCH 和 PCM 是否需要更新，方便之后的复用，具体有哪些任务呢？
  - 构建 PCH 和 PCM（这个也需要保存一些元信息来进行复用）
  - Preprocess 来获取 include 位置，或者扫描模块名
  - 构建 AST
  - 运行 CodeCompletion
  - 索引源文件，索引静态文件
  - 遍历 AST 响应各种 feature

- `Indexer` 这个类主要就是储存和索引有关的信息，保存 IncludeGraph，然后封装出一些接口对索引进行查询即可。像 GotoDefinition 这种请求可以尝试直接在 `Indexer` 中进行实现。这个类也负责对 header context 信息的维护和更新。

- `LSPConvert` 这个类主要是负责把我们内部的消息格式转换成 LSP 协议定义的格式，主要就是把 offset 转换成 line 和 column，以及进行路径映射，文件转换成 URI。除此之外，LSP 协议中一些相关的设置可能会影响转换的结果，相关的逻辑都在这里处理。

- `Server` 核心类型，负责处理一些初始化相关的工作，以及具体的响应消息驱动任务执行。

之前的模型还没有考虑到对文件的即时更改，这里应该也需要一个 class 来负责管理所有打开的文件。

    /// 暂时就一个源文件一个 PCH 了，想要 PCH 在不同源文件之间复用可太难了，
    /// 那有意思的来了，一个源文件上的一个 Action 是不是最多只能在池子里面运行一个呢？
    /// 其实不是，至少代码补全是独立的 ...

    /// 我们想要实现的逻辑
    /// 用户打开或者修改一个文件，我们要为它构建一个 AST
    /// 先检查有没有旧的 AST 构建任务，如果有的话需要给它取消掉
    /// 然后计算 Preamble 的 bound 和 content
    /// 如果有正在构建的 PCH 任务，检查一下它的内容是否有价值（检查 mtime 和 content 有效性）
    /// 如果有效的话，就 await 这个任务执行
    /// 否则取消这个 PCH 构建的任务，开启一个新的
    /// 如果没有正在构建的 PCH，就检索一下之前的 PCH
    /// 判断是否需要更新，如果需要就开启一个新的构建任务 然后 await 它，否则就用之前的 PCH 构建 AST
    /// 就好 如果有 pending 的 PCH 任务，检查一下它的信息，更新为新的就好



### clice 的索引 ... 

在 clice 中，索引可谓是最复杂的设计之一了，前后历经了多个版本的重构，随着语言服务器的需求越来越多，索引也变得越来越复杂。对于 clice 来说，索引就是对源文件的 AST 进行遍历然后选取一些感兴趣的内容进行特殊的组织，然后储存起来。以方便后续的读取。

首先索引的需求最早来自于实现 go to definition 这种请求，clangd 在其中就有很多这样的索引。

在对 clangd 索引进行一些精简以后，得出了为了实现 go to definition 这种请求的最小子集。

然后问题随之而来，首当其冲的问题就是如何处理头文件的索引？clangd 把头文件当成源文件，所以一个头文件大概只有一个索引。但是 clice 明确支持 header context，每个包含了该头文件的源文件在索引的时候都会为它生成一份索引。

显然，像 vector 这种文件在一个大型项目中几乎被所有的源文件包含（用户自己的头文件同理），如果我们不做任何的处理，那么索引的空间就会膨胀几千倍。显然这种量级上的差异我们是要考虑的，而且基于项目中绝大多数头文件都是自包含的所以它们的索引在不同源文件中的结构应该是完全相同的这个假设，我们也应该考虑合并头文件。

目前的方案很简单，把生成的索引序列化成二进制再对比 hash 判断是否相同，相同就保留一份，否则都保留。

但是基于二进制 hash 的处理并不那么合适，这要求两份索引完全相同才能合并。你可能会说非自包含头文件的结果在二进制索引中不就是完全相同的吗。在实际尝试之后，我发现事情并没有那么简单

考虑下面这个简单的示例

```cpp

```

在这两个文件中，对 `X<int, int>` 中 `X` 进行跳转的结果是不同的，一个是主模板一个是偏特化。跳转结果不同，自然索引也不相同，也就无法合并。但是显然，从用户的角度不应导致这样的结果。

为什么会这样的呢？这是因为 C++ 模板实例化是 lazy 的，Z 这种别名的形式并不要求模板实例化。而模板只有在实例化的时候才会选择具体的特化。所以 b.cpp 中没实例化就选择主模板了，c.cpp 中实例化了就选择偏特化了。

类似的情况还有 

```cpp
/// a.h
auto foo();

/// b.cpp
#include "a.h"

/// c.cpp
#include "a.h"

auto foo() {
    return 1;
}
```

a.h 也会在这两个头文件中产生不同的结果。因为其中一个推导出返回值类型了，另外一个则没有。类似的情况可能还有很多，而且由于我们要从模板实例化里面获取信息。或者用伪实例化器进行实例化，这种情况可能会进一步加重。基于二进制的完美比较感觉要被干掉了。

.... 类似的情况还发生在函数注释出现在 function definition 而非 declaration 上的时候，函数类型不同导致 USR 计算的结果不同？（这个也许可以在处理函数 USR 的时候忽略返回值类型来解决？这么来看这些导致 USR 上下文出问题的东西都可以采用单独的解法？需要进一步探究）

需要新加一种合并头文件的 header context 的办法，然后还要能够区分某个东西来自哪个上下文，需要更精细的比较方法。

宏和模板 ... ... 

终极目标是，对于宏和模板，我们都提供一个 preview 的功能。可以选择一个一个实例化来提供高亮等功能 


如何实现这个功能呢 ... ？对 top level 的宏展开来说，这个比较简单，因为 top level 的展开结果可以直接在 expanded token 里面找到。只需要在 render 的时候使用 Expansion Location 进行渲染，然后再收集起来就行了。对于非 top level 的宏展开呢？这个就很麻烦了 ... 需要再继续研究 clang 的内部机制看看是否能实现

对于模板实例化就遍历实例化的声明/定义，然后 render 就行了，模板占位符会变成 SubstTemplateT，这个是一个 sugar type，表示类型来自于模板实例化。


目前的索引格式有一个很大的问题是，include graph 加载不是 lazy 的，这个问题影响很严重。对于大项目的话，会导致 include 图过大...

需要修改索引元信息的记录方式

对于静态索引来说，使用
file -> indice path 的方式，一个源文件一个索引，完美对应。对于头文件的静态索引，我们会把它合并成一个（具体方式仍然需要讨论），源文件有它的所有包含的头文件的记录。头文件同样记录包含它的所有源文件的记录。

这样的话，加载 include 图就是 lazy 的了。打开某个文件，加载它的索引文件就可以查询到相关信息了，如果有必要可以递归的加载它依赖的文件。

对于打开的文件，我们维护一组动态索引

在满足一定条件后，会把动态索引更新到静态索引中。

接下来考虑一下 MergedHeaderIndex 的设计准则

谁在摧毁 header context？

