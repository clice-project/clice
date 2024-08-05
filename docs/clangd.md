首先分析一下项目架构，clice 本身是一个非常简单的模型，就是一个 client，实现了 [Language Server Protocol](https://microsoft.github.io/language-server-protocol/) 的 client。比如在 vscode 里面用的时候，对方发消息，然后我们回复指定内容即可。

那这个项目的难点主要在哪？

## 和庞大的 Clang 源码进行交互

目前进度：
- 成功基于 CompilerInstance 创建了简单的 CodeCompletionConsumer，可以进行简单的代码补全查询
- 了解到进行语义高亮的原理，先把所有 Tokens 记录下来，然后遍历 AST 的时候再根据语义信息（可以用 Location 作为 Key 来索引）去高亮，语义分析的时候 Tokens 肯定全有了，但是暂时不知道如何拿到 Tokens
- 成功拿到 Tokens，原来是 CodeCompletion 和 Tokens 不能同时用。在使用 Action 进行 Execute 后，可以从 CompilerInstance 里面拿到整个编译单元的 AST，遍历 AST（使用 RecursiveASTVisitor），然后再根据这个反向去渲染 Token 即可。
- 已经知道是由 PrecompiledPreamble 负责构建预编译头文件，clangd 里面的 Preamble(Preamble.cpp) 也都是指这个，但是。一个源文件编译最多依赖一个 pch，依赖关系是线性的，但是一个 module 可以依赖多个其它的 module，依赖关系是有向无环图。


TODO:
- 了解是什么 Preamble
- 了解 module 的工作机制
- 了解 FrontendAction 的使用
- 详细了解 Clang 前端的工作流程
- 了解启发式代码补全的原理
- 了解 AST 是如何被加载到内存中的

FIXME:
- 为每个 Token 都提供语义高亮: https://github.com/clangd/clangd/issues/1115
- no-self-contained: https://github.com/clangd/clangd/issues/45
- 提供更好的模板代码补全（需要在索引文件里面记录模板实例化），https://github.com/clangd/clangd/issues/443，然后补全的时候获取`.`号前面的表达式类型，之后再这里查找
- 支持模块：https://github.com/clangd/clangd/issues/1293
- https://github.com/clangd/clangd/issues/123 优化头文查找
## 性能优化

TODO: 寻找核心优化点
- 尽可能减少对 LLVM 源码的依赖，比如 json，hashmap 这种，提供方便换成第三方容器的接口，方便测试性能

一些讨论：
- 使用 LRU 缓存来优化语法树的查找与储存，参考 [Rust analyzer](https://github.com/rust-lang/rust-analyzer/pull/1382)，目前 clangd 使用嵌套 vector 来处理这个问题 [ASTNode](https://github.com/llvm/llvm-project/blob/main/clang-tools-extra/clangd/Protocol.h#L2017).
- [持久化储存](https://github.com/rust-lang/rust-analyzer/issues/4712)，指定缓存目录
- 做加法而不是做减法，默认只提供非常少的功能，以减轻内存使用负担，通过主动开启选项来提供更多功能

## 编制索引与缓存查询

TODO:
- 了解索引工作的实际调度过程
- 了解 AST 是以何种形式被储存到磁盘中的 

一些讨论：
- [支持离线索引](https://github.com/clangd/clangd/issues/587)
- 另外请见 https://discourse.llvm.org/t/using-background-and-static-indexes-simultaneously-for-large-codebases/3706/7


## 详细的支持功能列表

需要具体到哪些功能要做

比较大的特色是支持 module，这个需要重点支持，同时需要进一步阅读 LSProtocol 的文档，看看还有哪些功能需要支持。PCH 相关 ...。

一些可能有帮助的内容：
- 内联类型提示: https://github.com/clangd/clangd/issues/1535
- 生成函数实现（补全父类虚函数和未实现的成员函数）: https://github.com/clangd/clangd/issues/445
- 更详细的 Token 语义提示：https://github.com/clangd/clangd/issues/1115
- module support: https://github.com/clangd/clangd/issues/1293

一些必须要做的选项：
- 支持补全的时候不补全函数模板的<>
- 支持补全的时候不补全函数变量，模板变量，concept的占位符
- 支持 markdown 注释渲染
- 修复 quick fix 的位置问题
- 启发式模版补全（可选）
- 控制代码补全的时机，只有光标后面没有字母的时候才补全，不要改中间的词弹补全框，烦死了（可选）
- 如果可变参数类型名太长了，比如`tuple`之类的，则显式成`tuple<...>`然后可以点击展开

希望支持的一些功能：
- 可视化宏展开（类似 VS 里面那个功能）
- 插件系统，支持用户编写一些简单的脚步来扩展功能（代码生成 .etc）

# clangd 源码阅读（从 ClangMain 开始一点一点阅读）


## 第一阶段

`ClangMain(tool/ClangMain.cpp)` 里面没什么重要的内容，主要是 LLVM 的初始化和设置一些命令行 Option，主要调用了 `ClangServerLSP(ClangServerLSP.h)` 的 `run` 函数，这个 `run` 函数本身主要调用了 `Transport` 的 `loop` 函数，即开启服务器的事件循环

`Transport(Transport.h)` 本身是一个抽象类，通过不同的子类来实现，可以发送不同格式的消息。在 `ClangMain` 里面根据不同的情况使用不同的 `Transport` 子类，默认是 `JSONTransport(JSONTransport.cpp)`，大部分方法都没什么好看的，就是设置一些协议格式，然后发送消息（似乎是通过标准输入输出流进行通信，不过这个不重要），我们主要看 `loop` 函数。

`loop` 函数主要调用 `handleMessage` 这个私有方法来处理消息，这个私有方法则主要是把最后的处理交给 `MessageHandler`，`MessageHandler` 是 `Transport` 的一个成员抽象类，`ClangServerLSP` 实现了一个 `MessageHandler(ClangServerLSP.cpp-176)` 用于处理消息，对应的成员是 `MsgHandler`，在构造函数中初始化。下面主要看这个 `MsgHandler` 的处理逻辑。

`MsgHandler` 主要有三个函数 `onNotify`，`onCall` 和 `onReply`。分别表示 响应通知，响应请求和响应回复。三个函数的逻辑大体是相似的，都是 RPC 调用，根据函数名调用对应的函数，然后返回执行结果（找不到就报错）。接下来就主要看 `Handlers` 这个成员，他负责储存所有的处理函数。

`Handlers` 的类型是 `LSPBinder::RawHandlers`，在 `ClangServerLSP` 的构造函数中把 `Handlers` 这个成员绑定给了 `Bind`（`LSPBinder` 类型），`LSPBinder` 的 `method`，`notification` 和 `command` 成员函数的作用是，分别往对应的 `handler` 里面注册函数。在 `ClangServerLSP` 的构造函数中注册了 `ClangdLSPServer::onInitialize` 这个函数，可以猜测，实际的初始化工作都是在这里完成的，接下来我们主要看 `onInitialize` 这个函数的逻辑。

这里执行了很多初始化的逻辑，暂时没有细看，不知道都是干嘛用的。注册成员函数是在 `ClangdLSPServer` 的 `bindMethods` 方法中完成的，它注册了所有需要用到的方法，第一阶段的阅读到这里暂时结束，接下来要针对每一个模块看了。

## 第二阶段

接下来主要看`ClangServer(ClangServer.h)`这个类型，`ClangServerLSP`里面的绝大多数触发函数，只是对这个类对应函数的简单包装。先重点看一下它的`BackgroundIdx`和`TUScheduler`这两个成员。前者负责对文件进行索引，把结果储存到磁盘上。后者负责管理和加载 AST 到内存中，便于后续的操作。

一切的一切都从`ClangServer::addDocument`这个函数开始，处理客户端发过来的数据，然后调用`TUScheduler`的`update`函数，来加载到内存中。之后再调用`BackgroundIdx`的`boostRelated`函数，储存对应的索引文件。接下来先分析`TUScheduler`这个类。所有的 AST 都储存在 Files 这个成员变量里，它的类型是`llvm::StringMap<std::unique_ptr<FileData>>`，它同时有一个`ASTCache`类型的成员变量`IdleASTs`，LRU 储存一些 AST，用于快速查找。

`TUScheduler`的`update`函数主要创建了一个`ASTWorkerHandle`，然后调用它的`update`函数来处理这个事情。其实就是发起一个异步任务，更新对应的 AST 内容。值得注意的是 update 函数似乎都只是更新相关文件的状态，而具体的 AST 或者 Preamble 的创建则都是在第一次使用的时候，即 runWithAST 和 runWithPreamble 中。

TODO: 查看 runWithAST 和 runWithPreamble 的具体实现，PreambleThread 负责构建 PCH

Clang 的 AST 的实际解析发生在 ParsedAST.cpp 的 build 函数，ParsedAST 本身会储存一个 CompilerInstance 实例。

!!! 原来 CodeCompletion 和 TokenCollector 不能同时获取，这也就意味着如果需要完成代码补全和语义高亮似乎需要多次遍历 AST ......

## 注意事项

注意：FeatureModule 这个东西没啥用，感觉是个废弃的功能，之后记得扔了。

## 一些已经解决的问题

SemanticHighlight：通过 SynatxOnlyAction 和 TokenCollector 可以拿到所有 Tokens，之后再遍历一遍语法树，分别处理每个语法树元素的高亮即可（可以通过 Location 查询 Token）。另外要注意，关键字高亮优先级最大，记得处理遍历语法树时候未处理完的 Token。

