#pragma once

// support basic reflection through template meta programming
#include <bit>
#include <tuple>
#include <string_view>
#include <source_location>

namespace clice::impl {

struct Any {
    consteval Any(std::size_t);

    template <typename T>
    consteval operator T () const;
};

template <typename T, std::size_t N>
consteval auto test() {
    return []<std::size_t... I>(std::index_sequence<I...>) {
        return requires { T{Any(I)...}; };
    }(std::make_index_sequence<N>{});
}

template <typename T, std::size_t N = 0>
consteval auto member_count() {
    if constexpr(test<T, N>() && !test<T, N + 1>()) {
        return N;
    } else {
        return member_count<T, N + 1>();
    }
}

template <typename T>
struct Wrapper {
    T value;

    constexpr Wrapper(T value) : value(value) {}
};

template <Wrapper T>
consteval auto member_name() {
    std::string_view name = std::source_location::current().function_name();
#if __GNUC__ && (!__clang__) && (!_MSC_VER)
    std::size_t start = name.rfind("::") + 2;
    std::size_t end = name.rfind(')');
    return name.substr(start, end - start);
#elif __clang__
    std::size_t start = name.rfind(".") + 1;
    std::size_t end = name.rfind('}');
    return name.substr(start, end - start);
#elif _MSC_VER
    std::size_t start = name.rfind("->") + 2;
    std::size_t end = name.rfind('}');
    return name.substr(start, end - start);
#else
    static_assert(false, "Not supported compiler");
#endif
}

template <std::size_t count, typename Object>
constexpr auto collcet_members(Object&& object) {
    // clang-format off
    if constexpr(count == 0) {
        return std::tuple{};
    } else if constexpr(count == 1) {
        auto&& [a] = object;
        return std::tuple{&a};
    } else if constexpr(count == 2) {
        auto&& [a, b] = object;
        return std::tuple{&a, &b};
    } else if constexpr(count == 3) {
        auto&& [a, b, c] = object;
        return std::tuple{&a, &b, &c};
    } else if constexpr(count == 4) {
        auto&& [a, b, c, d] = object;
        return std::tuple{&a, &b, &c, &d};
    } else if constexpr(count == 5) {
        auto&& [a, b, c, d, e] = object;
        return std::tuple{&a, &b, &c, &d, &e};
    } else if constexpr(count == 6) {
        auto&& [a, b, c, d, e, f] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f};
    } else if constexpr(count == 7) {
        auto&& [a, b, c, d, e, f, g] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g};
    } else if constexpr(count == 8) {
        auto&& [a, b, c, d, e, f, g, h] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g, &h};
    } else if constexpr(count == 9) {
        auto&& [a, b, c, d, e, f, g, h, i] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g, &h, &i};
    } else if constexpr(count == 10) {
        auto&& [a, b, c, d, e, f, g, h, i, j] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g, &h, &i, &j};
    } else if constexpr(count == 11) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k};
    } else if constexpr(count == 12) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k, &l};
    } else if constexpr(count == 13) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k, &l, &m};
    } else if constexpr(count == 14) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k, &l, &m, &n};
    } else if constexpr(count == 15) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k, &l, &m, &n, &o};
    } else if constexpr(count == 16) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k, &l, &m, &n, &o, &p};
    } else if constexpr(count == 17) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k, &l, &m, &n, &o, &p, &q};
    } else if constexpr(count == 18) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k, &l, &m, &n, &o, &p, &q, &r};
    } else if constexpr(count == 19) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k, &l, &m, &n, &o, &p, &q, &r, &s};
    } else if constexpr(count == 20) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k, &l, &m, &n, &o, &p, &q, &r, &s, &t};
    } else if constexpr(count == 21) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k, &l, &m, &n, &o, &p, &q, &r, &s, &t, &u};
    } else if constexpr(count == 22) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k, &l, &m, &n, &o, &p, &q, &r, &s, &t, &u, &v};
    } else if constexpr(count == 23) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k, &l, &m, &n, &o, &p, &q, &r, &s, &t, &u, &v, &w};
    } else if constexpr(count == 24) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k, &l, &m, &n, &o, &p, &q, &r, &s, &t, &u, &v, &w, &x};
    } else if constexpr(count == 25) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k, &l, &m, &n, &o, &p, &q, &r, &s, &t, &u, &v, &w, &x, &y};
    } else if constexpr(count == 26) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k, &l, &m, &n, &o, &p, &q, &r, &s, &t, &u, &v, &w, &x, &y, &z};
    } else if constexpr(count == 27) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, _0] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k, &l, &m, &n, &o, &p, &q, &r, &s, &t, &u, &v, &w, &x, &y, &z, &_0};
    } else if constexpr(count == 28) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, _0, _1] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k, &l, &m, &n, &o, &p, &q, &r, &s, &t, &u, &v, &w, &x, &y, &z, &_0, &_1};
    } else if constexpr(count == 29) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, _0, _1, _2] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k, &l, &m, &n, &o, &p, &q, &r, &s, &t, &u, &v, &w, &x, &y, &z, &_0, &_1, &_2};
    } else if constexpr(count == 30) {
        auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, _0, _1, _2, _3] = object;
        return std::tuple{&a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k, &l, &m, &n, &o, &p, &q, &r, &s, &t, &u, &v, &w, &x, &y, &z, &_0, &_1, &_2, &_3};
    } else {
        static_assert(count <= 30, "Not supported member count");
    }
    // clang-format on
}

template <typename T>
struct Storage {
    inline static T value;
};

template <auto value>
consteval auto enum_name() {
    std::string_view name = std::source_location::current().function_name();
#if __GNUC__ || __clang__
    std::size_t start = name.find('=') + 2;
    std::size_t end = name.size() - 1;
#elif _MSC_VER
    std::size_t start = name.find('<') + 1;
    std::size_t end = name.rfind(">(");
#else
    static_assert(false, "Not supported compiler");
#endif
    name = name.substr(start, end - start);
    start = name.rfind("::");
    return start == std::string_view::npos ? name : name.substr(start + 2);
}

template <typename T, std::size_t N = 0>
consteval auto enum_max() {
    constexpr auto value = std::bit_cast<T>(static_cast<std::underlying_type_t<T>>(N));
    if constexpr(enum_name<value>().find(")") == std::string_view::npos)
        return enum_max<T, N + 1>();
    else
        return N;
}

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

template <typename Source, typename Target>
using replace_cv_ref_t = typename replace_cv_ref<Source, Target>::type;

}  // namespace clice::impl

namespace clice {

template <typename... Ts>
struct Record;

template <typename T>
constexpr inline bool is_record_v = false;

template <typename... Ts>
constexpr inline bool is_record_v<Record<Ts...>> = true;

#define CLICE_RECORD(name, ...)                                                                                        \
    struct name##Body;                                                                                                 \
    using name = Record<__VA_ARGS__, name##Body>;                                                                      \
    struct name##Body

template <typename T>
concept Reflectable = std::is_aggregate_v<std::decay_t<T>> && std::is_default_constructible_v<std::decay_t<T>>;

template <Reflectable Object, typename Callback>
constexpr void for_each(Object&& object, const Callback& callback) {
    using T = std::decay_t<Object>;
    if constexpr(is_record_v<T>) {
        T::for_each(std::forward<Object>(object), callback);
    } else {
        constexpr auto count = impl::member_count<T>();
        auto members = impl::collcet_members<count>(object);
        constexpr auto static_members = impl::collcet_members<count>(impl::Storage<T>::value);
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (callback(impl::member_name<std::get<Is>(static_members)>(), *std::get<Is>(members)), ...);
        }(std::make_index_sequence<count>{});
    }
}

// reflectable struct definition
template <typename... Ts>
struct Record : Ts... {
    template <typename Object, typename Callback>
    static void for_each(Object&& object, const Callback& callback) {
        (clice::for_each(static_cast<impl::replace_cv_ref_t<Object&&, Ts>>(object), callback), ...);
    }
};

template <typename T>
    requires std::is_enum_v<T>
constexpr auto enum_name(T value) {
    constexpr auto count = impl::enum_max<T>();
    constexpr auto names = []<std::size_t... Is>(std::index_sequence<Is...>) {
        return std::array{impl::enum_name<static_cast<T>(Is)>()...};
    }(std::make_index_sequence<count>{});
    return names[static_cast<std::size_t>(value)];
}

};  // namespace clice
