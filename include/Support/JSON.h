#pragma once

#include <array>
#include <vector>
#include <string_view>

#include "ADT.h"
#include "TypeTraits.h"
#include "Enum.h"
#include "Struct.h"

#include "llvm/Support/JSON.h"

namespace clice::json {

using namespace llvm::json;

/// Specialize this struct to provide custom serialization and deserialization for a type.
template <typename V>
struct Serde;

/// Check if the serde if given type is stateful.
template <typename V>
concept stateful_serde = requires {
    Serde<V>::stateful;
    requires Serde<V>::stateful;
};

/// Serialize an object to a JSON value.
template <typename V, typename... Serdes>
json::Value serialize(const V& v, Serdes&&... serdes) {
    if constexpr(!stateful_serde<V>) {
        return Serde<V>::serialize(v);
    } else if constexpr(sizeof...(Serdes) > 0) {
        using S = Serde<V>;
        if constexpr((std::is_same_v<S, std::remove_cvref_t<Serdes>> || ...)) {
            auto try_each =
                [&]<typename First, typename... Rest>(auto& self, First&& first, Rest&&... rest) {
                    if constexpr(std::is_same_v<std::remove_cvref_t<First>, S>) {
                        /// If we already have a direct serde, use it.
                        return std::forward<First>(first).serialize(v);
                    } else if constexpr(sizeof...(rest) > 0) {
                        /// Try the next serde.
                        return self(self, std::forward<Rest>(rest)...);
                    } else {
                        static_assert(dependent_false<V>, "Unexpected control flow");
                    }
                };
            return try_each(try_each, std::forward<Serdes>(serdes)...);
        } else {
            /// Otherwise, pass the serdes to the next serde.
            return Serde<V>::serialize(v, std::forward<Serdes>(serdes)...);
        }
    } else {
        static_assert(dependent_false<V>, "Stateful serde requires at least one serde");
    }
}

/// Deserialize a JSON value to an object.
template <typename T, typename... Serdes>
T deserialize(const json::Value& value, Serdes&&... serdes) {
    if constexpr(!stateful_serde<T>) {
        return Serde<T>::deserialize(value);
    } else if constexpr(sizeof...(Serdes) > 0) {
        using S = Serde<T>;
        if constexpr((std::is_same_v<S, std::remove_cvref_t<Serdes>> || ...)) {
            auto try_each =
                [&]<typename First, typename... Rest>(auto& self, First&& first, Rest&&... rest) {
                    if constexpr(std::is_same_v<std::remove_cvref_t<First>, S>) {
                        /// If we already have a direct serde, use it.
                        return std::forward<First>(first).deserialize(value);
                    } else if constexpr(sizeof...(rest) > 0) {
                        /// Try the next serde.
                        return self(self, std::forward<Rest>(rest)...);
                    } else {
                        static_assert(dependent_false<T>, "Unexpected control flow");
                    }
                };
            return try_each(try_each, std::forward<Serdes>(serdes)...);
        } else {
            /// Otherwise, pass the serdes to the next serde.
            return Serde<T>::deserialize(value, std::forward<Serdes>(serdes)...);
        }
    } else {
        static_assert(dependent_false<T>, "Stateful Serde requires at least one serde");
    }
}

template <>
struct Serde<std::nullptr_t> {
    static json::Value serialize(std::nullptr_t) {
        return json::Value(nullptr);
    }

    static std::nullptr_t deserialize(const json::Value& value) {
        assert(value.kind() == json::Value::Null && "Expect null");
        return nullptr;
    }
};

template <>
struct Serde<std::string> {
    using V = std::string;

    static json::Value serialize(const V& v) {
        return json::Value(v);
    }

    static V deserialize(const json::Value& value) {
        assert(value.kind() == json::Value::String && "Expect a string");
        return value.getAsString().value().str();
    }
};

template <>
struct Serde<bool> {
    static json::Value serialize(bool v) {
        return json::Value(v);
    }

    static bool deserialize(const json::Value& value) {
        assert(value.kind() == json::Value::Boolean && "Expect boolean");
        return value.getAsBoolean().value();
    }
};

template <clice::integral I>
struct Serde<I> {
    static json::Value serialize(I v) {
        return json::Value(static_cast<int64_t>(v));
    }

    static I deserialize(const json::Value& value) {
        assert(value.kind() == json::Value::Number && "Expect number");
        return static_cast<I>(value.getAsInteger().value());
    }
};

template <typename E>
    requires std::is_enum_v<E>
struct Serde<E> {
    static json::Value serialize(E v) {
        return json::Value(static_cast<int64_t>(v));
    }

    static E deserialize(const json::Value& value) {
        assert(value.kind() == json::Value::Number && "Expect number");
        return static_cast<E>(value.getAsInteger().value());
    }
};

template <clice::floating_point F>
struct Serde<F> {
    static json::Value serialize(F v) {
        return json::Value(static_cast<double>(v));
    }

    static F deserialize(const json::Value& value) {
        assert(value.kind() == json::Value::Number && "Expect number");
        return static_cast<F>(value.getAsNumber().value());
    }
};

template <>
struct Serde<double> {
    static json::Value serialize(float v) {
        return json::Value(v);
    }

    static float deserialize(const json::Value& value) {
        assert(value.kind() == json::Value::Number && "Expect number");
        return value.getAsNumber().value();
    }
};

template <std::size_t N>
struct Serde<char[N]> {
    static json::Value serialize(const char (&v)[N]) {
        return json::Value(llvm::StringRef(v, N));
    }
};

template <>
struct Serde<llvm::StringRef> {
    using V = llvm::StringRef;

    static json::Value serialize(const V& v) {
        return json::Value(v.str());
    }

    static V deserialize(const json::Value& value) {
        assert(value.kind() == json::Value::String && "Expect string");
        return value.getAsString().value();
    }
};

template <>
struct Serde<std::string_view> {
    using V = std::string_view;

    static json::Value serialize(const V& v) {
        return json::Value(llvm::StringRef(v.data(), v.size()));
    }

    static V deserialize(const json::Value& value) {
        assert(value.kind() == json::Value::String && "Expect string");
        return value.getAsString().value();
    }
};

template <typename T, std::size_t N>
struct Serde<std::array<T, N>> {
    using V = std::array<T, N>;

    constexpr inline static bool stateful = stateful_serde<T>;

    template <typename... Serdes>
    static json::Value serialize(const V& v, Serdes&&... serdes) {
        json::Array array;
        for(const auto& element: v) {
            array.push_back(json::serialize(element, std::forward<Serdes>(serdes)...));
        }
        return array;
    }

    template <typename... Serdes>
    static V deserialize(const json::Value& value, Serdes&&... serdes) {
        assert(value.kind() == json::Value::Array && "Expect array");
        V array;
        for(std::size_t i = 0; i < N; ++i) {
            array[i] =
                json::deserialize<T>((*value.getAsArray())[i], std::forward<Serdes>(serdes)...);
        }
        return array;
    }
};

template <typename T>
struct Serde<std::vector<T>> {
    using V = std::vector<T>;

    constexpr inline static bool stateful = stateful_serde<T>;

    template <typename... Serdes>
    static json::Value serialize(const V& v, Serdes&&... serdes) {
        json::Array array;
        for(const auto& element: v) {
            array.push_back(json::serialize(element, std::forward<Serdes>(serdes)...));
        }
        return array;
    }

    template <typename... Serdes>
    static V deserialize(const json::Value& value, Serdes&&... serdes) {
        assert(value.kind() == json::Value::Array && "Expect array");
        V array;
        for(const auto& element: *value.getAsArray()) {
            array.emplace_back(json::deserialize<T>(element, std::forward<Serdes>(serdes)...));
        }
        return array;
    }
};

template <typename T>
struct Serde<llvm::ArrayRef<T>> {
    using V = llvm::ArrayRef<T>;

    constexpr inline static bool stateful = stateful_serde<T>;

    /// Only refl serialization.
    template <typename... Serdes>
    static json::Value serialize(const V& v, Serdes&&... serdes) {
        json::Array array;
        for(const auto& element: v) {
            array.push_back(json::serialize(element, std::forward<Serdes>(serdes)...));
        }
        return array;
    }
};

template <typename E>
    requires refl::special_enum<E>
struct Serde<E> {
    static json::Value serialize(const E& e) {
        return json::Value(e.value());
    }

    static E deserialize(const json::Value& value) {
        return E(json::deserialize<typename E::underlying_type>(value));
    }
};

template <refl::reflectable T>
struct Serde<T> {
    constexpr inline static bool stateful =
        refl::member_types<T>::apply([]<typename... Ts> { return (stateful_serde<Ts> || ...); });

    template <typename... Serdes>
    static json::Value serialize(const T& t, Serdes&&... serdes) {
        json::Object object;
        refl::foreach(t, [&](std::string_view name, auto& member) {
            object.try_emplace(llvm::StringRef(name),
                               json::serialize(member, std::forward<Serdes>(serdes)...));
        });
        return object;
    }

    template <typename... Serdes>
    static T deserialize(const json::Value& value, Serdes&&... serdes) {
        T t = {};
        if constexpr(!std::is_empty_v<T>) {
            assert(value.kind() == json::Value::Object && "Expect an object");
            refl::foreach(t, [&](std::string_view name, auto&& member) {
                if(auto v = value.getAsObject()->get(llvm::StringRef(name))) {
                    member = json::deserialize<std::remove_cvref_t<decltype(member)>>(
                        *v,
                        std::forward<Serdes>(serdes)...);
                }
            });
        }
        return t;
    }
};

}  // namespace clice::json
