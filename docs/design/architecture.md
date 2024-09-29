"architectural issue" here actually means the issue which can be solved but need to make a lot of modification to clangd. Currently, such big modification can be hard to push because of lack resource in clangd as mentioned [here](https://github.com/clangd/clangd/issues/1690#issuecomment-1619735578). I am a university student, also a c++ enthusiast and a vscode user.  So I have a lot of time that I can dedicate to things I am interested in.. At first, I just want to wonder whether I can make some efforts to clangd. But gradually, I found it could be better to rewrite a new LSP. After evaluating the project scale, I think it's possible to complete for just one person (about 4w~ lines and ccls is also written by one person). Then I start to dive into clangd source code and learn the clang base. So far, I have known how to build preamble, how to build AST, how to traverse AST and so on. All of them are done with clang's C++ API. Next step is putting them together into final program. However, before the actual implementation, I need to plan out its structure.

At first, how to manage message? LSP is also a type of server, and the common event-driven model is sufficient. I plan to use libuv as the event library, with the main thread handling requests and distributing tasks, while the thread pool executes the actual tasks. This is slightly different from clangd's current model; clangd doesn't use a thread pool, but rather creates a new thread for each file and uses semaphore to limit the number of active threads.

Then we need to handle TU. I decided to use a different policy with clangd, which is not to build the preamble for the file until the first edit, and headers will reuse the AST in the source file by default, also until first edit. In this way, we can reduce a lot of memory usage, speed up the loading (building preamble always needs more time compared to normal AST building) and support non self contained header. This can be very useful, people actually only need to modify a few files in big projects like LLVM. And today I found Sam also reported this [issue](https://github.com/clangd/clangd/issues/391). Another important problem is how to interact with C++20 modules, I am still exploring this.

Next is about indexing, current indexer in clangd seems to be efficient enough, so I think I would reuse it and make only some small modifications to record more information. Also, I am wondering whether we can modify the absolute path to relative path in the index, so issue like [support offline background index preparation](https://github.com/clangd/clangd/issues/587)。It shouldn't be hard to implement.

Following is discussion on some specific LSP features:

## Semantic Highlight

There are several issues around it.

- [provide semantic information for each token](https://github.com/clangd/clangd/issues/1115)
- [no AST node for explicit template instantiations](https://github.com/clangd/clangd/issues/358)
- [class name does not get a coloring in constructor call ](https://github.com/clangd/clangd/issues/501)
- [store implicit specializations as distinct symbols in the index, with their own refs](https://github.com/clangd/clangd/issues/536)
- [distinguish between type template parameter and non-type template parameter highlightings](https://github.com/clangd/clangd/issues/787)
- [modifiers are wrong in semantic token for destructor](https://github.com/clangd/clangd/issues/872)

Mainly about wrong or not enough information in the semantic highlight. I would like to support detail information for each token, then we can solve all of them.

## Hint

- [simplify inlay hints](https://github.com/clangd/clangd/issues/824)
- [make the type hint length limit configurable](https://github.com/clangd/clangd/issues/1357)
- [support go-to-definition on type names in inlay hints](https://github.com/clangd/clangd/issues/1535)
- [use namespace alias but not original namespace name](https://github.com/clangd/clangd/issues/542)
- [support folding range on directive](https://github.com/clangd/clangd/issues/1661)
- [documentation for struct member fields not working correctly](https://github.com/clangd/clangd/issues/564)
- [make the hover box more compact](https://github.com/clangd/clangd/issues/747)
- [padding inlay hints](https://github.com/clangd/clangd/issues/923)
- [struct members and enum values are not shown on type hover](https://github.com/clangd/clangd/issues/959)
- [show length of string literal in hover](https://github.com/clangd/clangd/issues/1016)
- [show type hints for init-captures](https://github.com/clangd/clangd/issues/1163)

Most of these problems are about hint. Providing options to allow users to configure them will be fine.

## Action

- [add implementation](https://github.com/clangd/clangd/issues/445)
- [revert if](https://github.com/clangd/clangd/issues/466)
- [move constructor argument into member](https://github.com/clangd/clangd/issues/454)
- [modify function parameters](https://github.com/clangd/clangd/issues/460)
- [expand macro one level](https://github.com/clangd/clangd/issues/820)

Providing more actions will be helpful.

## Code Completion

- [improve code completion inside template](https://github.com/clangd/clangd/issues/443)
- [resolve the type of lambda auto parameters](https://github.com/clangd/clangd/issues/493)
- [don't include template completion when inside the template class](https://github.com/clangd/clangd/issues/678)
- [can I disable snippet](https://github.com/clangd/clangd/issues/727)
- [no completion for definition of non-public member functions](https://github.com/clangd/clangd/issues/880)
- [outside function definition won't complete default arguments](https://github.com/clangd/clangd/issues/753)
- [postfix completions](https://github.com/clangd/clangd/issues/850)
- [list free functions alongside methods in autocompletion](https://github.com/clangd/clangd/issues/893)
- [type information lost during auto deduction in templates](https://github.com/clangd/clangd/issues/897)
- [completion member field listing order w.r.t. C++20 designated initializers](https://github.com/clangd/clangd/issues/965)
- [completion of pointers to functions and function objects](https://github.com/clangd/clangd/issues/968)

These issues around code completion could be hardest to resolve. We need to modify the `Parser` and `SemaComplete` in clang to support them. Some are about templates, improving `HeuristicResolver` will resolve them. 