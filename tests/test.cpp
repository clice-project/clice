#include <AST/Resolver.h>

template <typename T>
using void_t = void;

template <typename T, typename U>
struct replace_first_arg;

template <template <typename...> typename K, typename T, typename U>
struct replace_first_arg<K<T>, U> {
    using type = K<U>;
};

template <typename A, typename T, typename = void>
struct rebind : replace_first_arg<A, T> {};

template <typename A, typename T>
struct rebind<A, T, void_t<typename A::template rebind<T>::other>> {
    using type = typename A::template rebind<T>::other;
};

template <typename A>
struct allocator_traits {
    using value_type = typename A::value_type;

    template <typename T>
    using rebind_alloc = typename rebind<A, T>::type;
};

template <typename A, typename = typename A::value_type>
struct alloc_traits {
    using base = allocator_traits<A>;
    using value_type = typename base::value_type;
    using reference = value_type&;

    template <typename T>
    struct rebind {
        using other = typename base::template rebind_alloc<T>;
    };
};

template <typename T, typename A = std::allocator<T>>
struct vector {
    using alloc_type = typename alloc_traits<A>::template rebind<T>::other;
    using reference = typename alloc_traits<alloc_type>::reference;
};

int main() {
    llvm::outs() << "Hello, World!" << '\n';
    return 0;
}
