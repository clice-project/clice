- collect information from preprocessor, figure out what is the proper to store these information, e.g. macros, pragmas, directives, comments, etc.

- schedule files, ...

- use `TreeTransform` to simplify `DependentNameResolver`

- refactor `clang/lib/SemaComplete.cpp`

给 AST Features 分个类



- Go to Declaration - readonly
- Go to Definition - readonly

跳转到被引用实体的声明或者定义。有些实体不区分声明和定义，比如宏，别名和命名空间声明，此时这两个函数效果是一样的。由于 C++ 允许多次向前声明，所以 Go to Declaration 可能返回多个结果。但是定义只有一个，所以 Go to Definition 一般只会返回一个结果。有些时候当我们浏览子类的虚函数实现的时候，想要跳转到父类的实现，此时可以对该函数上的 `virtual`, `override` 或者 `final` 使用上述命令。 
 
- Go to type definition
跳转到类型定义，该调用会直接跳转到变量/字段的类型定义

- Go to implementation
常用于跳转到虚函数实现，部分情况下。特别的，如果 clice 能发现你这个类型是一个 CRTP 类型，那么它也可以进行对应的跳转。

#
