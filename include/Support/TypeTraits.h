#pragma once

#include <tuple>
#include <type_traits>

namespace clice {

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

template <typename Tuple, template <typename> typename Map>
struct tuple_map;

template <typename... Ts, template <typename> typename Map>
struct tuple_map<std::tuple<Ts...>, Map> {
    using type = std::tuple<typename Map<Ts>::type...>;
};

/// Map the types in the tuple to another type with the given template template argument.
template <typename Tuple, template <typename> typename Map>
using tuple_map_t = typename tuple_map<Tuple, Map>::type;

template <typename T>
struct identity {
    using type = T;
};

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

template<typename T>
concept floating_point = std::is_floating_point_v<T>;

}  // namespace clice
