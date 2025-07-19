代办事项，逐步完成

先完善 PCH 构建吧，主要任务有如下
1. 完善三种模式下的 PCH 构建和使用
    - 正常的源文件模式，把前面几个 header 构建成 PCH，然后使用
    - header 模式，通过给定的 include 链计算出头文件前面的 preamble，并构建隐式 PCH 添加到前面（有两种计算来源，一种是单独运行预处理指令搜集包含图和预处理过后的文本等，一种是从现存的索引中计算出包含图）
    - module 模式，同源文件模式，但是处理 module 预处理指令
2. 收集 top level decls
3. 考虑如何在 PCH 模式下位各个 feature 提供支持
   1. SemanticToken 简单遍历声明即可，对于跳过的 preamble 部分，仅凭 lex 即可处理（除非 include 的参数是宏，这个必须要 preamble 中宏展开的信息）。
   2. DocumentSymbol（遍历 top level symbols 即可）
   3. FoldingRange preamble 部分仅凭 lex 可以处理，剩余部分从 AST 中获取
   4. InlayHint 从 AST 中进行处理
   5. Hover 使用 SelectionTree 从 AST 中进行处理
   6. DocumentLink，理论上可以从 PCH 中获取到跳过的 preamble 但是目前还没有找到合适的办法（还有一种办法是在索引中储存 include diretive 的信息，这个是更为推荐的解法，可以为了未来的 readonly 模式铺路，这要求索引 PCH，如何管理 PCH 的索引是一个问题）。
   7. CodeCompletion 和 SignatureHelp，直接运行 Consumer 可以解决
   8. CodeAction 使用 SelectionTree 可以解决

这些问题解决之后 PCH 即 ok 喽！！！

这里其实还需要对 Compiler 下的一些设置进行较大规模的改动，具体的细节明天再想。

今天的任务
1. 在 ASTConsumer 中收集 top level decls，并且存到 ASTInfo 里面，使用 atomic bool 来决定是否来停止 parse
2. 修改 diagnostic consumer 收集错误，并处理 create invocation 和 crate instance 的错误
3. 重新调整 ASTInfo 以适应不同的 AST 请求

所以 clice 会运行哪些 FrontendAction 来确保正常的运行？

- PreprocessOnlyAction 用来仅预处理文件，扫描模块依赖或者获取 include 图
- GeneratePCHAction 用于生成 PCH
- GenerateReducedModuleInterfaceAction 用于生成 PCM
- SyntaxOnlyAction 用来

六种不同的 AST 能访问的接口不同


| 表格                       | 预处理 | 构建 PCH | 构建 PCM | 代码补全 | 编译静态文件 | 编译主文件 |
| -------------------------- | ------ | -------- | -------- | -------- | ------------ | ---------- |
| SM, PP, diretives          | √      | √        | √        | √        | √            | √          |
| AST, Sema                  | ×      | √        | √        | √        | √            | √          |
| Resolver                   | ×      | √        | √        | √        | √            | √          |
| TokenBuffer                | ×      | √        | √        | ×        | √            | √          |
| Interested file or decls   | ×      | ×        | ×        | √        | ×            | √          |
| Diagnostics and clang tidy | ×      | ×        | ×        | ×        | ×            | √          |
| May need stop              | ×      | √        | √        | √        | √            | √          |

可以看到关系非常复杂，不太好能用静态类型来表达，所以我决定还是把它们都写到一个类里面，然后为其中一些类附带上对应的 assert 来处理了。


| 场景                     | 描述                                                          | Clang (基于 inode)                                                         | GCC (基于内容/时间戳)                    | MSVC (基于路径)                    |
| :----------------------- | :------------------------------------------------------------ | :------------------------------------------------------------------------- | :--------------------------------------- | :--------------------------------- |
| 硬链接                   | path/a.h 和 other/b.h 指向同一个 inode。                      | 相同。UniqueID 完全一致。                                                  | 相同。内容、大小和修改时间都一致。       | 不同。规范化后的路径字符串不同。   |
| 符号链接                 | path/a.h 是指向 target/b.h 的符号链接。                       | 相同。解析链接后获取目标的 UniqueID。                                      | 相同。解析链接后获取目标的内容/时间戳。  | 不同。链接和目标的规范化路径不同。 |
| 副本文件                 | path/a.h 和 other/b.h 是内容相同的副本（inode 不同）。        | 不同。UniqueID 不同。                                                      | 相同。内容、大小和修改时间匹配。         | 不同。规范化后的路径不同。         |
| 路径大小写差异           | 在大小写不敏感的文件系统（如 Windows）上，File.h vs. file.h。 | 不同。Clang 的路径处理默认是大小写敏感的。                                 | 不同。GCC 的路径处理默认是大小写敏感的。 | 相同。路径规范化后字符串相同。     |
| 网络文件系统 (ID 不稳定) | 文件位于 Windows 文件 ID 不稳定的网络共享上。                 | 可能不同 (漏报)。LLVM 的现代策略（路径哈希）缓解了此问题，但旧版本会失败。 | 相同。不受影响，因为它依赖内容/时间戳。  | 相同。不受影响，因为它依赖路径。   |


# 任务一：改进 CompilationUnit
1. 修改 CompilationUnit 这个结构体，使其能访问 ...，SM 变成指针而非引用
2. 修改 compile 实现，把一些必要的参数移动到 create invocation 里面（一切设置 invocation 的都应当移动到这里），
3. 为每个需要支持 pause 的 ASTFrontendAction 都使用一个 wrapper 的 Action，重写 createASTConsumer，创造一个 wrapper 的 ASTConsumer 包装原本的 ASTConsumer，每次都检查原子变量来决定是否要停止编译。
4. 继续把 CompilationUnit 里面的一些其它的成员函数进行清理，重命名和补充文档

# 任务三：改进文件路径处理
1. 现在 clice 的文件路径处理很不规范，有很多地方都自己搞了一个 pool，比如 IncludeGraph，CompilationDatabase 以及每个 CompilationUnit。需要明确一种方式来管理 path，比如全局的 PathPool？需要仔细研究在多线程的情况下这是否是一个好的方案。（还要考虑到之后可能添加的 path-map 功能）

2. 第二个问题就是关于文件唯一性的了，如何判断唯一性？这个很麻烦很麻烦。不过初步决定，对于 workspace 下的路径，都使用相对的路径可以保持唯一。问题在于 system 目录下的文件怎么办呢？考虑和 clang 一样采取基于 inode 的方法？可是写入到索引文件里面的时候又该怎么处理呢？需要进一步探究。

# 任务四
## 代码补全

1. 基本的补全和过滤已经搞定，不过现在的补全很 trivial，没有 snippet 考虑喜欢 snippet（函数，模板 等，支持不同风格），还有就是 document 等等的渲染也需要进一步研究
2. signature helper 还没开始研究，不过这个感觉太 trivial 可以之后再说？

## 实现 query driver 的思路

根本原因，编译器使用的默认标准库路径往往是硬编码，而编译 clice 使用的路径大概没有。然后呢很多时候用户使用的是标准库的默认路径，造成了不一致，这些参数也不在 CDB 中，造成了问题。clice 的标准库搜索路径需要与 driver 保持一致。

##
CompilationDatabase 职责

- cache command，高效储存 CDB（大部分编译命令的 canonical 形式都是相同的），过滤没用的选项（比如 -c -o -module-deps 这些），合并分离的选项，比如 -I foo 合并成 -Ifoo，通过这些手段可以将 GB 为单位的 CDB 压缩几十倍。（实现上，使用 clang driver 的 optTable parse 然后过滤即可）
- 注入 query driver 和 resource dir，这个是 lazy 注入的，为某个文件获取的时候才注入。具体的实现细节需要进一步研究
- 支持 config 文件里面的 `append` 和 `remove`，待研究
- 支持 fallback command，当新建源文件时，command 尚不可用，需要一种好的算法来寻找合适的 command，简单就使用同目录下的，可能需要某种优先级算法（有个简单的办法，不使用路径做映射，而是目录，map 到一个 vector 这样按照层次搜就行了，同事还维护一个使用）
- 支持增量更新，当某个 CDB 文件更新时，支持和上一次的进行对比，并给出命令变更的源文件

## 需要考虑


# 移除
- 待考虑，模块名到模块路径映射要在这里存吗，还是单独一个类管理模块依赖关系呢，经过思考之后我觉得单独使用一个类来管理模块依赖比较好，不只是模块名和模块路径的映射。可能还有 include 图之类的。CDB 专门负责管理 CDB 就好。