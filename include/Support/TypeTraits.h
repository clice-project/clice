#pragma once

#include <tuple>
#include <type_traits>

namespace clice {

template <typename T>
struct identity {
    using type = T;
};

template <typename T>
using identity_t = T;

template <typename... Ts>
struct type_list {
    constexpr static auto apply(auto&& lambda) {
        return lambda.template operator()<Ts...>();
    }

    using to_tuple = std::tuple<Ts...>;
};

/// Turn a tuple into a type list.
/// @param Tuple The tuple to convert.
/// @param Map The mapping function to apply to each type in the tuple.
/// @param isalias If isalias is false, mapping result is `typename Map<T>::type`.
template <typename Tuple, template <typename> typename Map = identity_t, bool isalias = true>
struct tuple_to_list;

template <typename... Ts, template <typename> typename Map>
struct tuple_to_list<std::tuple<Ts...>, Map, true> {
    using type = type_list<Map<Ts>...>;
};

template <typename... Ts, template <typename> typename Map>
struct tuple_to_list<std::tuple<Ts...>, Map, false> {
    using type = type_list<typename Map<Ts>::type...>;
};

template <typename Tuple, template <typename> typename Map = identity_t, bool isalias = true>
using tuple_to_list_t = typename tuple_to_list<Tuple, Map, isalias>::type;

template <typename Tuple>
struct tuple_uniuqe {
    using type = Tuple;
};

/// Uniuqe the types in the tuple.
template <typename Tuple>
using tuple_uniuqe_t = typename tuple_uniuqe<Tuple>::type;

template <typename T, typename... Ts>
    requires (!std::is_same_v<T, Ts> && ...)
struct tuple_uniuqe<std::tuple<T, Ts...>> {
    using type = decltype(std::tuple_cat(std::declval<std::tuple<T>>(),
                                         std::declval<tuple_uniuqe_t<std::tuple<Ts...>>>()));
};

template <typename T, typename... Ts>
    requires (std::is_same_v<T, Ts> || ...)
struct tuple_uniuqe<std::tuple<T, Ts...>> {
    using type = tuple_uniuqe_t<std::tuple<Ts...>>;
};

template <typename Source, typename Target>
struct replace_cv_ref;

template <typename T, typename Target>
struct replace_cv_ref<T&, Target> {
    using type = Target&;
};

template <typename T, typename Target>
struct replace_cv_ref<T&&, Target> {
    using type = Target&&;
};

template <typename T, typename Target>
struct replace_cv_ref<const T&, Target> {
    using type = const Target&;
};

template <typename T, typename Target>
struct replace_cv_ref<const T&&, Target> {
    using type = const Target&&;
};

/// Replace the cv-qualifiers and reference of Source with Target.
/// For example, `replace_cv_ref_t<const int&, double>` is `const double&`.
template <typename Source, typename Target>
using replace_cv_ref_t = typename replace_cv_ref<Source, Target>::type;

template <typename T, template <typename...> typename HKT>
constexpr bool is_specialization_of = false;

template <template <typename...> typename HKT, typename... Args>
constexpr bool is_specialization_of<HKT<Args...>, HKT> = true;

template <typename T>
constexpr inline bool dependent_false = false;

template <typename T>
concept integral =
    std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char> &&
    !std::is_same_v<T, wchar_t> && !std::is_same_v<T, char16_t> && !std::is_same_v<T, char32_t>;

template <typename T>
concept floating_point = std::is_floating_point_v<T>;

// traits for function types
template <typename Fn>
struct function_traits {
    static_assert(dependent_false<Fn>, "unsupported function type");
};

#define FUNCTION_TRAITS_SPECIALIZE(...)                                                            \
    template <typename R, typename... Args>                                                        \
    struct function_traits<R(Args...) __VA_ARGS__> {                                               \
        using return_type = R;                                                                     \
        using args_type = std::tuple<Args...>;                                                     \
    };

FUNCTION_TRAITS_SPECIALIZE()
FUNCTION_TRAITS_SPECIALIZE(&)
FUNCTION_TRAITS_SPECIALIZE(const)
FUNCTION_TRAITS_SPECIALIZE(const&)
FUNCTION_TRAITS_SPECIALIZE(noexcept)
FUNCTION_TRAITS_SPECIALIZE(& noexcept)
FUNCTION_TRAITS_SPECIALIZE(const noexcept)
FUNCTION_TRAITS_SPECIALIZE(const& noexcept)

#undef FUNCTION_TRAITS_SPECIALIZE

template <typename T>
using function_return_t = typename function_traits<T>::return_type;

template <typename T>
using function_args_t = typename function_traits<T>::args_type;

template <typename T>
struct member_traits;

template <typename M, typename C>
struct member_traits<M C::*> {
    using member_type = M;
    using class_type = C;
};

template <typename T>
using member_type_t = typename member_traits<T>::member_type;

template <typename T>
using class_type_t = typename member_traits<T>::class_type;

}  // namespace clice
