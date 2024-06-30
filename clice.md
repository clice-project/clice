`clangd.md`侧重于对 clangd 源码的分析，仅仅是分析里面的关键部分，作为我们编写代码的参考。但是最终我们是要有我们自己的架构的，所以有必要自顶向下的对项目模块进行一下规划。

整体上来看，clice 的模型很简单，只是一个实现了 LSP 协议的 Server，所以一般的服务器模型也适用于它。

## Overview

首先整个服务器底层需要有线程池和事件循环来处理事件（异步逻辑），具体有哪些事件后文会详细讨论。我们打算使用 C++20 来编写，所以可以使用协程来简化异步代码的编写。相比于 clangd 中的回调函数套回调函数，这可以大大提高代码可维护性。

## Event

现在我们要讨论有哪些事件需要处理，这里就要根据 LSP 来具体分析了。

### Server Lifecycle

这里主要是处理和服务器生命周期相关的消息，比如初始化，关闭，等等。这些消息需要最优先处理。由于 cLang 上游代码时不时可能会崩溃，所以重启对于 clice 来说是比较常见的。

>理想情况是对线程进行隔离，一个线程的编译器挂了不影响其它的线程，这个还需要进一步研究。

值得注意的是 clice 希望额外支持插件功能，所以需要利用 LSP 中的 registerCapability 这个消息格式。

### Text Document Synchronization

这个是和文档同步相关的消息，比如文档打开，关闭，修改等等。这个消息是最频繁的，所以需要尽可能的优化。

>目前 clangd 在一个文件打开的时候就会在后台发起编译这个文件的预编译头的任务，具体的策略需要进一步研究。


### Language Features

首先对 LSP 支持的功能进行概览：LSP 3.17 currently: 

- Goto Declaration：跳转到声明
- Goto Definition：跳转到定义
- Goto Type Definition：跳转到类型定义
- Goto Implementation：跳转到实现
- Find References：查找所有引用
- Prepare Call Hierarchy：没搞懂
- Call Hierarchy Incoming Calls：没搞懂
- Call Hierarchy Outgoing Calls：没搞懂
- Prepare Type Hierarchy：没搞懂
- Type Hierarchy Supertypes：没搞懂
- Type Hierarchy Subtypes：没搞懂
- Document Highlights：没搞懂
- Document Link：没搞懂
- Document Link Resolve：没搞懂
- Hover：悬停提示
- Code Lens：没搞懂
- Code Lens Refresh Request：没搞懂
- Folding Range：把某段代码折叠起来
- Selection Range：没搞懂
- Document Symbols：没搞懂
- Semantic Tokens：用于语义高亮
- Inline Value：没搞懂
- Inline Value Refresh：没搞懂
- Inlay Hint：用于内嵌提示，比如函数参数或者`auto`的类型
- Inlay Hint Resolve：没搞懂
- Inlay Hint Refresh：刷新内嵌提示
- Monikers：没搞懂
- Completion：代码补全
- Completion Item Resolve：解决重载函数的代码补全
- PublishDiagnostics Notification：发出诊断信息
- Pull Diagnostics：没搞懂
- Signature Help Request：请求函数签名信息
- Code Action：重构等操作（还有那个 quick fix）
- Code Action Resolve：没搞懂
- Document Color：没搞懂
- Color Presentation：没搞懂
- Document Formatting：格式化
- Document Range Formatting：只格式化某个部分
- Document on Type Formatting：没搞懂
- Rename：重命名
- Prepare Rename：解决重命名
- Linked Editing Range：没搞懂

这些任务从最终实现的角度来说可以主要分成三种：
1. CodeCompletion 这个需要利用 CodeCompletionConsumer 调用 Clang 提供的接口来实现，然而我们实际上可以做一些更加复杂的分析（clangd 目前没有做）。比如判断当前的是不是在 Template 语境下从而决定补全`sizeof`的时候要不要补全`...`。在补全成员的时候，似乎我们也可以获取`expr.f`中的父对象的类型，从而根据它的类型来做一些补全。有待进一步研究。
2. Semantic Tokens 等基于当前 AST 的操作，则是遍历 AST 渲染 Token 即可。
3. 剩下很多的，例如 Find References 等等等查询功能，都是在已经索引好的文件中进行查询，不需要对语法树进行什么改动。