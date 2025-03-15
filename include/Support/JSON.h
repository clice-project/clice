#pragma once

#include <array>
#include <vector>
#include <ranges>
#include <string_view>

#include "Ranges.h"
#include "TypeTraits.h"
#include "Enum.h"
#include "Struct.h"

#include "llvm/Support/JSON.h"

namespace clice::json {

using namespace llvm::json;

/// Specialize this struct to provide custom serialization and deserialization for a type.
template <typename V>
struct Serde;

template <typename V>
concept serializable = requires { sizeof(Serde<V>); };

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
struct Serde<json::Value> {
    static json::Value serialize(auto&& value) {
        return json::Value(std::forward<decltype(value)>(value));
    }

    static json::Value deserialize(auto&& value) {
        return json::Value(std::forward<decltype(value)>(value));
    }
};

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
struct Serde<std::nullopt_t> {
    static json::Value serialize(std::nullopt_t) {
        return json::Value(nullptr);
    }

    static std::nullopt_t deserialize(const json::Value& value) {
        assert(value.kind() == json::Value::Null && "Expect null");
        return std::nullopt;
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
struct Serde<const char*> {
    static json::Value serialize(const char* v) {
        return json::Value(llvm::StringRef(v));
    }
};

template <std::size_t N>
struct Serde<char[N]> {
    static json::Value serialize(const char (&v)[N]) {
        return json::Value(llvm::StringRef(v, N));
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

template <std::size_t N>
struct Serde<llvm::SmallString<N>> {
    using V = llvm::SmallString<N>;

    static json::Value serialize(const V& v) {
        return json::Value(v.str());
    }

    static V deserialize(const json::Value& value) {
        assert(value.kind() == json::Value::String && "Expect string");
        return V{value.getAsString().value().str()};
    }
};

template <map_range Range>
struct Serde<Range> {
    using key_type = typename Range::key_type;
    using mapped_type = typename Range::mapped_type;

    constexpr inline static bool stateful = stateful_serde<key_type> || stateful_serde<mapped_type>;

    template <typename... Serdes>
    static json::Value serialize(const Range& range, Serdes&&... serdes) {
        json::Object object;
        for(const auto& [key, value]: range) {
            if constexpr(std::is_constructible_v<json::Object::key_type, decltype(key)>) {
                object.try_emplace(key, json::serialize(value, std::forward<Serdes>(serdes)...));
            } else {
                object.try_emplace(
                    llvm::formatv("{}", json::serialize(key, std::forward<Serdes>(serdes)...)),
                    json::serialize(value, std::forward<Serdes>(serdes)...));
            }
        }
        return object;
    }

    template <typename... Serdes>
    static Range deserialize(const json::Value& value, Serdes&&... serdes) {
        assert(value.kind() == json::Value::Object && "JSON must be object");
        Range range;
        for(auto& [name, value]: *value.getAsObject()) {
            if constexpr(std::is_constructible_v<key_type, decltype(name)>) {
                range.try_emplace(
                    name,
                    json::deserialize<mapped_type>(value, std::forward<Serdes>(serdes)...));
            } else {
                if(auto key = json::parse(name)) {
                    range.try_emplace(
                        json::deserialize<key_type>(std::move(*key),
                                                    std::forward<Serdes>(serdes)...),
                        json::deserialize<mapped_type>(value, std::forward<Serdes>(serdes)...));
                }
            }
        }
        return range;
    }
};

template <set_range Range>
struct Serde<Range> {
    using key_type = typename Range::key_type;

    constexpr inline static bool stateful = stateful_serde<key_type>;

    template <typename... Serdes>
    static json::Value serialize(const Range& range, Serdes&&... serdes) {
        json::Array array;
        for(const auto& element: range) {
            array.emplace_back(json::serialize(element, std::forward<Serdes>(serdes)...));
        }
        return array;
    }

    template <typename... Serdes>
    static Range deserialize(const json::Value& value, Serdes&&... serdes) {
        assert(value.kind() == json::Value::Array && "JSON must be array");
        Range range;
        for(auto& element: *value.getAsArray()) {
            range.emplace(json::deserialize<key_type>(element, std::forward<Serdes>(serdes)...));
        }
        return range;
    }
};

template <sequence_range Range>
struct Serde<Range> {
    using value_type = typename Range::value_type;

    constexpr inline static bool stateful = stateful_serde<value_type>;

    template <typename... Serdes>
    static json::Value serialize(const Range& range, Serdes&&... serdes) {
        json::Array array;
        for(const auto& element: range) {
            array.emplace_back(json::serialize(element, std::forward<Serdes>(serdes)...));
        }
        return array;
    }

    template <typename... Serdes>
    static Range deserialize(const json::Value& value, Serdes&&... serdes) {
        assert(value.kind() == json::Value::Array && "JSON must be array");
        Range range;
        for(auto& element: *value.getAsArray()) {
            range.emplace_back(
                json::deserialize<value_type>(element, std::forward<Serdes>(serdes)...));
        }
        return range;
    }
};

template <refl::reflectable_enum E>
struct Serde<E> {
    static json::Value serialize(const E& e) {
        return json::Value(e.value());
    }

    static E deserialize(const json::Value& value) {
        return E(json::deserialize<typename E::underlying_type>(value));
    }
};

template <typename T>
constexpr inline bool is_optional_v = false;

template <typename T>
constexpr inline bool is_optional_v<std::optional<T>> = true;

template <refl::reflectable_struct T>
struct Serde<T> {
    constexpr inline static bool stateful =
        refl::member_types<T>::apply([]<typename... Ts> { return (stateful_serde<Ts> || ...); });

    template <typename... Serdes>
    static json::Value serialize(const T& t, Serdes&&... serdes) {
        json::Object object;
        refl::foreach(t, [&]<typename Field>(std::string_view name, const Field& field) {
            if constexpr(is_optional_v<Field>) {
                if(field) {
                    object.try_emplace(llvm::StringRef(name),
                                       json::serialize(*field, std::forward<Serdes>(serdes)...));
                }
            } else {
                object.try_emplace(llvm::StringRef(name),
                                   json::serialize(field, std::forward<Serdes>(serdes)...));
            }
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
