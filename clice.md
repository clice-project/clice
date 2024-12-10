clice 目前进度概览

clice 的代码主要可以分成互不相交的两部分

# 服务器端

这部分代码逻辑主要位于 Server 目录下，主要负责处理客户端的请求，调度任务执行，如更新 PCH，PCM 和 AST，索引文件等等。

主要实现和目前进度：目前已经使用 C++20 协程对 libuv 进行了封装，主线程为事件循环线程。一般来说在接受到一个请求后会把它转发给对应的 `OnXXX` 函数进行处理，这个过程中会自动将 json 反序列化成对应请求参数的结构体。编译文件和遍历 AST 都是相对耗时的操作，我们会将其调度到 libuv 的线程池上执行。当收到 `didOpen` 或者 `didChange` 请求时，我们就会开始为该文件构建 AST。其它后续的请求往往依赖于前一步构建生成的 AST，这样请求之间就有了依赖关系。为了处理请求之间的依赖关系，我们为每个文件维护一个消息队列，当一个新的请求到来的时候，如果当前文件空闲，那就直接执行，如果正忙，那就加入队列中等待。**注意，clang 的 AST 即使是只遍历（只读）在多线程下也不是线程安全的，所以即使 AST 构建完了并且后面的 AST 对 AST 都是只读的，那我们也只能一个个把它们调度到线程池上执行。**

按照上面这个简单的模型，目前已经和客户端成功实现了通信，成功在客户端显式出了我们的 SemanticTokens 渲染的结果。

但是这只是最初步的模型，实际上还有很多地方需要进行完善：
1. 首先就是实现 CDB（compile_commands.json）文件的增量更新，用户很可能在服务器运行的时候修改 CDB 文件，我们需要监听文件更新事件，并做出相应的处理。例如当一个文件的编译命令改变的时候，我们需要更新它的 PCH。至于监听文件实现这里有两种方式，一种是通过 libuv 的 API 进行，另一种是通过 LSP 协议，让客户端进行监听，当发生更新的时候发生通知到服务器。嗯，还没确定用哪一种，感觉用 libuv 内置的就好。还有就是 clang 提供的读取 CDB 文件的 API 效果太差，对于大文件表现不太好，注意到 CDB 文件其实就是 json 文件，所以我们自己解析好了。
2. 调度索引工作进行，为了提供跨文件的符号查找（例如 go-to-definition），我们需要索引整个项目的文件。最理想的状况是，用户在启动 clice 之前已经预先索引好了所有的文件，这样我们只要维护活跃文件的索引更新就行了，但是往往情况并不是这样的。我们需要在服务器空闲的时候，进行后台索引，也就是索引任务的执行优先级应该低于正常 LSP 请求。具体的实现的话，大概就是结合 libuv 提供的 idle（提供一个回调函数在空闲时执行），自己维护一个低优先级的任务队列，然后在里面处理索引的逻辑。另外整个项目的文件可能非常多，我们可能需要某种启发式的逻辑，先索引活跃文件附近的文件，例如在同一个文件夹里面的。
3. 支持 header context，目前我们把头文件当成一个 TU 进行处理，但是首先头文件在 CDB 文件里面没有编译选项，其次它可能并不能独立编译。只有当它出现在某个源文件中的时候，依赖于这个头文件之前包含的其他头文件里面的定义才能正常编译。所谓 header context 就是为头文件支持不同的源文件上下文。我们会在合适的时候尝试切换头文件上下文，当然也允许用户主动查询和切换它的上下文。
4. 支持 C++20 modules，module 的编译流程和一个普通的文件其实没太大区别。主要区别就是，module 的编译依赖于 PCM 而不是 PCH。还有就是，模块文件里面只有 module name，但是我们不知道这个 name 对应的 module 的文件路径。于是我们需要再加载 CDB 的时候，预处理一遍整个项目中的文件，建立 module name -> file path 的映射，从而在编译 module 的时候我们能找到依赖的 module 对应的源文件，进而把编译进行下去。注意 PCM 的依赖关系可以是网状的，所以我们可以在线程池里面并发构建多个 PCM，如果有必要的话。
5. 支持 LSP 中的取消请求功能，有时候如果客户端在短时间间隔内发送了重复的请求的话，它可能会把前面那个请求取消掉。我们需要支持取消请求，具体的做法大概也就是自定义一个消息队列（目前我们使用的是 libuv 内置的 async 来把事件投送到它的消息队列里面）。

这些请求做完之后，服务器端基本就完成了，没有什么其它的需求需要处理了。


# 具体的请求实现

Server 中对对应请求的处理一般就是直接转发到 Feature 里面的函数。

我们将要支持以下特性

- GotoDefinition/GotoDeclaration/GotoTypeDefinition/GotoImplementation/FindReferences 用于提供符号跳转
- OutgoingCalls/IncomingCalls/SuperTypes/SubTypes 用于提供符号关系

上面这几个特性的实现方式是很统一的，即查询索引文件。目前索引的主要代码逻辑已经完成，只需要编写更多的测试即可。

- SemanticTokens 用于源文件种代码的语义高亮

SemanticTokens 和 Index 的实现逻辑是有一些相似之处的，于是我抽象出了一个接口给它俩使用，即 `SemanticVisitor`（位于`Semantic.h`），提供了一个接口用于遍历源文件中的所有符号位置。

- CodeCompletion 用于代码补全
- SignatureHelp 用于重载函数的签名提示

这两个请求的实现有一些特殊，它们需要单独跑一次编译，而不是依赖于 AST 的执行结果。

- DocumentHighlight 用于突出显式源码中的某个文件
- DocumentLink 用于提供头文件的链接，使其可跳转
- DocumentSymbols 用于实现在 vscode 左侧的 outline 中显式的嵌套符号关系
- FoldingRange 用于实现代码折叠
- Hover 用于实现鼠标悬停提示
- InlayHint 用于源码中嵌入信息提示

目前只实现了 SemanticTokens 和 CodeCompletion 的部分逻辑。不过后面几个请求都比较好实现，可能就 InlayHint 稍微复杂一点。

# 一些缺失的工具函数
- 我们可以从 clang 种拿到代码的注释，但是这个注释往往是 doxygon 和 markdown 混合的注释，我们需要一个 parser 来解析注释，并转换成纯正的 markdown
- 对 Location 进行转换，Location 里面含有 line 和 column，但是这个根据源文件位置编码的不同 UTF8，UTF16，UTF32 会有不同的结果，clang 内部使用的是 UTF8，所以我们最好进行一些统一转换

      - name: Get submodule commit hash
        id: submodule_commit_hash
        run: |
            echo "hash=$(git submodule status deps/llvm | awk '{ print $1 }')" >> $GITHUB_ENV
            echo "::set-output name=hash::$(git submodule status deps/llvm | awk '{ print $1 }')"
            
      - name: Cache LLVM source and build
        uses: actions/cache@v3
        with:
          path: deps/llvm
          key: ${{ runner.os }}-llvm-${{ github.sha }}-submodule-${{ steps.submodule_commit_hash.outputs.hash }}
          restore-keys: |
            ${{ runner.os }}-llvm-${{ github.sha }}-submodule-


      - name: Build LLVM
        run: |
          python3 ./scripts/build-llvm-dev.py

      # Step 5: Always run the test build script
      - name: Build and Test
        run: |
          bash ./scripts/build-dev-test.sh
          bash ./build/bin/clice-tests --test-dir=./tests