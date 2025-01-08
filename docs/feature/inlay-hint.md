# Inlay Hint 

## Supported Case
1. Type hint for variable declared with `auto` keywords in `function/if/for/structure-binding`.
2. Return type hint for `function/lambda` body.
3. Parameter name in function call expression.
5. Block end hint for `struct/class/enum` declaration and `function/lambda` body.
6. Array index for elements in initialize list expression.
7. Value of `sizeof` and `alignof` in `struct/class` defination. 

There is a configurable option, and some examples for above cases, see [struct clice::config::InlayHintOption](../../include/Feature/InlayHint.h) for more infomation. 
