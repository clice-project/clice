# clice 

这个文件详细描述了 clice 的项目架构以及诸多实现细节。

# Overview

clice 是一个 C++ 的 [LSP](https://microsoft.github.io/language-server-protocol/)，旨在解决 clangd 中现存的诸多问题。下面是我认为 clangd 中一些现存的需要迫切去解决的问题：

## Index

- [preamble-less first build](https://github.com/clangd/clangd/issues/391)
- [C++20 module support](https://github.com/clangd/clangd/issues/1293)
- [support non self contained header](https://github.com/clangd/clangd/issues/45)
- [add an option to build ASTs for files in the CDB on startup](https://github.com/clangd/clangd/issues/589)
- [support offline background index preparation](https://github.com/clangd/clangd/issues/587)
- [give the user some way to control the order in which files are indexed](https://github.com/clangd/clangd/issues/523)
- [improve template instantiation and dependent types](https://github.com/clangd/clangd/issues/1095)

## Completion

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


## Semantic Highlight

- [provide semantic information for each token](https://github.com/clangd/clangd/issues/1115)
- [no AST node for explicit template instantiations](https://github.com/clangd/clangd/issues/358)
- [class name does not get a coloring in constructor call ](https://github.com/clangd/clangd/issues/501)
- [store implicit specializations as distinct symbols in the index, with their own refs](https://github.com/clangd/clangd/issues/536)
- [distinguish between type template parameter and non-type template parameter highlightings](https://github.com/clangd/clangd/issues/787)
- [modifiers are wrong in semantic token for destructor](https://github.com/clangd/clangd/issues/872)

## Action

- [add implementation](https://github.com/clangd/clangd/issues/445)
- [revert if](https://github.com/clangd/clangd/issues/466)
- [move constructor argument into member](https://github.com/clangd/clangd/issues/454)
- [modify function parameters](https://github.com/clangd/clangd/issues/460)
- [expand macro one level](https://github.com/clangd/clangd/issues/820)

## 