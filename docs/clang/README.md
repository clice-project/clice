在编写 clice 这个项目的时候，很大一个挑战就是和 clang 的源码进行交互。尽管 clang 一开始就被设计为模块化的项目，但是由于文档的匮乏，以及其本身就是一个生命周期非常长的项目了，不可避免的导致不同模块间的耦合程度加重，导致基于它编写相关的代码的时候较为困难。本文的旨在为 clice 项目中使用到的 clang 源码部分提供详细的介绍，方便阅读。

# Overview

TODO:

我们的目标是，基于 clang 的代码，自己编译出一个编译器前端程序出来，可以产生 AST 以便于我们使用，中端和后端这里就省略了。

# CompilerInstance

```cpp
class CompilerInstance { /* ... */ }
```

这个类其实就代表一个 C++ 编译器实例，通过它我们就能完成实际代码的编译工作。它是个可默认构造的类型

```cpp
auto instance = std::make_unique<clang::CompilerInstance>();
```

但是不要被表象迷惑了，这样默认构造出来的`instance`其实是不能直接用，如果你在 Debug 模式下构建，你会得到一大堆断言失败的错误。`CompilerInstance`有非常多的`set*`方法，只有在这些方法都正确的调用之后，才能执行最后的编译。下面就一步步让我们看看有哪些成员要被正确设置。

# Diagnostic

编译器如何处理错误？各种错误，比如解析命令行可能出错，预处理阶段可能出错，语法分析语义分析阶段也可能出错，如何呈现报错信息呢？这就是本小结要讨论的问题。

核心的类型主要有四个

`DiagnosticsEngine`用于管理所有和诊断相关的对象。

```cpp
class DiagnosticOptions{ /* ... */ }
```

`DiagnosticOptions`用于设置诊断选项。

```cpp
class DiagnosticConsumer{ /* ... */ }
```

`DiagnosticConsumer`用于处理诊断信息。可以重写这个类的方法来自定义诊断信息的处理方式。有一个默认的实现`TextDiagnosticPrinter`，它会将诊断信息输出到指定的流中。

```cpp
class DiagnosticIDs { /* ... */ }
```

`DiagnosticIDs` 负责管理诊断消息的唯一标识符。每个诊断消息都有一个唯一的 ID，用于在代码中引用特定的诊断消息。

```cpp
class DiagnosticsEngine{ /* ... */ }
```

`DiagnosticsEngine`是一个诊断引擎，用于生成和管理诊断消息。

创建

```cpp
clang::DiagnosticIDs* ids = new clang::DiagnosticIDs();
clang::DiagnosticOptions* diag_opts = new clang::DiagnosticOptions();
clang::DiagnosticConsumer* consumer = new clang::TextDiagnosticPrinter(llvm::errs(), diag_opts);
clang::DiagnosticsEngine* engine = new clang::DiagnosticsEngine(ids, diag_opts, consumer);
```

准备好`DiagnosticsEngine`之后，就可以设置给`instance`了，注意参数是一个裸指针，`instance`会获取它的所有权。

```cpp
instance->setDiagnostics(engine);
```

# CompilerInvocation

```cpp
class CompilerInvocation { /* ... */ }
```

这个类型用于向编译器传递一些信息，比如编译选项，输入文件等等，它同样是一个可默认构造的类型

```cpp
auto invocation = std::make_shared<clang::CompilerInvocation>();
```

同样，这样构造出来的对象是不能直接用的。可以使用`CompilerInvocation::CreateFromArgs`从一组命令行选项来初始化它。

```cpp
std::vector<const char*> args = {"-Xclang", "-c", "main.cpp"};
clang::CompilerInvocation::CreateFromArgs(*invocation, args, instance->getDiagnostics());
```

通过它的`getFrontendOpts`方法，我们可以获取到解析过的编译选项。

```cpp
auto& opts = invocation->getFrontendOpts();
```

clang 提供了代码补全的接口，如果想使用的话需要设置相应的`getFrontendOpts`

```cpp
auto& codeCompletionAt = opts.CodeCompletionAt;
codeCompletionAt.FileName = "main.cpp";
codeCompletionAt.Line = 10;
codeCompletionAt.Column = 4;
```

效果上和使用这个编译选项是类似的

```shell
clang++ -cc1 -fsyntax-only -code-completion-at main.cpp:10:4 main.cpp
```

准备好`invocation`之后就可以设置给`instance`了

```cpp
instance->setInvocation(std::move(invocation));
```

# Target

target 也就是我们常说的目标，这会影响最终生成的代码，例如不同平台的类型大小和对齐等等因素不同，那么`sizeof`等运算符求值得到的结果也就不同。不过由于往往在编译选项中就会默认指定 target 了，我们不需要再去自己创建，只需要

```cpp
if(!instance->createTarget()) {
    llvm::errs() << "Failed to create target\n";
    std::terminate();
}
```

就会自动根据当前的编译选项来创建对应的 target 了。

# FileManager and SourceManager

```cpp
if(auto manager = instance->createFileManager()) {
    instance->createSourceManager(*manager);
} else {
    llvm::errs() << "Failed to create file manager\n";
    std::terminate();
}
```

# Preprocessor

```cpp
class Preprocessor { /* ... */ }
```

Preprocessor 就是预处理器，负责源文件的预处理工作，比如宏展开，条件编译等等。同样，基于先前的设置，我们可以方便的使用`createPreprocessor`来创建一个预处理器，而不需要自己用 Preprocessor 来构造，省去了一些不必要的麻烦。

```cpp
instance->createPreprocessor(clang::TranslationUnitKind::TU_Complete);
auto& preprocessor = instance->getPreprocessor();
```

clang 暴露给了我们一些钩子在预处理的过程中获取一些信息。例如可以重写 PPCallbacks 里面的一些方法来获取一些信息。例如下面这个示例就是在打印每次宏展开的时候输出一些信息。clice 就通过这种方式来获取一个源文件中的头文件信息。

```cpp
using namespace clang;

class Callback : public PPCallbacks {
public:
    /// Called by Preprocessor::HandleMacroExpandedIdentifier when a
    /// macro invocation is found.
    void MacroExpands(const Token& MacroNameTok,
                      const MacroDefinition& MD,
                      SourceRange Range,
                      const MacroArgs* Args) override {
        llvm::outs() << "MacroExpands: " << MacroNameTok.getIdentifierInfo()->getName() << "\n";
    }

    /// Hook called whenever a macro definition is seen.
    void MacroDefined(const Token& MacroNameTok, const MacroDirective* MD) override {
        llvm::outs() << "MacroDefined: " << MacroNameTok.getIdentifierInfo()->getName() << "\n";
    }
};
```

之后只需要将这个 Callback 设置给 Preprocessor 就可以了

```cpp
preprocessor.addPPCallbacks(std::make_unique<Callback>());
```



This class is used to simplify a dependent type, so that we can do more things on it. e.g. code
completion. Dependent type is a type that depends on a template parameter.

For example:

```cpp
template<typename T>
void foo(std::vector<T> vec);
```

`std::vector<T>` is a dependent type, because it depends on the template parameter `T`. Beacuse it has not been instantiated yet, we can't determine which specialization it will use(if have), even can't not determine whether the instantiation is valid or not. Beacuse of this, we can't not provide **precise** code completion for it.

Note that I emphasis the word **precise**. In fact, in most of the cases, not very precise is tolerable. It's better than nothing. We can only consider the main template and provide code completion for it. 






