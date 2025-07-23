# Concept

So what is a [dependent name](https://en.cppreference.com/w/cpp/language/dependent_name)? It is a name that depends on a template parameter. For example, 

```cpp
struct X {
    constexpr inline static int value = 0;
};

template <typename T = X>
auto foo() {
    return T::value;
}
```

In the above code, `T::value` is a dependent name because it depends on the template parameter `T`. The compiler cannot know what `T::value` is until the template is instantiated. This is the simplest example of a dependent name. In actual code, dependent names can be much more complex, like `std::vector<std::vector<T>>::value_type::value`. Overall, if the prefix of a nested name contains a template parameter, then the nested name is a dependent name.

C++ standard will take a dependent name as a expression by default, if you want to indicate it is a type, you need to use `typename` keyword to tell the compiler it is a type. Otherwise, you will get a compile error. For example, 

```cpp
template <typename T = std::vector<int>>
void foo() {
    T::value_type x;  // error
    typename T::value_type x;  // okay
}
```

Sometimes, the name may be neither a type nor a value, it could be a template. In this case, you can use `template` keyword to tell the compiler it is a template. For example, 

```cpp
struct X {
    template <typename T>
    constexpr inline static T value = sizeof(T);
};

template <typename T = X>
auto foo() {
    return T::template value<int>;
}
```

Here, `value` in `X` is a template static variable, so you need to use `template` keyword to tell the compiler it is a template.

# Issues around Dependent Name

Here are some issues complaining about clangd's poor performance with templates, and most of them are caused by dependent names. So what exactly is the problem with dependent names?

Consider the following code:

```cpp
template<typename T>
auto foo() {
    return T::va^lue;
}
```

`^` represents the cursor position. If at this point, the user tries to click the "Go to Definition" button, they will get nothing. As we mentioned before, the compiler cannot know what `T::value` is until the template is instantiated.

The same problem also exists in the code completion. For example, 

```cpp
template<typename T>
auto foo() {
    T::va^
}
```

Similarly, nothing will be shown in the code completion list. The problem is that users don't actually need such a fully generic solution. They may only instantiate templates with a few types, but they have no way to tell the LSP. The new language feature `concept` introduced in C++ can help solve this problem, but it is still limited.

Okay, okay, you say such a direct dependent name cannot be resolved. What about the following code? Why can I still not get the code completion list? Of course, `vec2[0]` is a vector, right?

```cpp
template<typename T>
auto foo() {
    std::vector<std::vector<T>> vec2;
    vec2[0].^
}
```

According to the C++ standard, the type of `vec2[0]` is `std::vector<std::vector<T>>::reference`. Oh, damn, still a dependent name.

# Heuristic

