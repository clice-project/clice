#pragma once

#include <nlohmann/json.hpp>

#include <Support/Reflection.h>

namespace clice::impl {

template <typename T>
constexpr inline bool is_optional_v = false;

template <typename T>
constexpr inline bool is_optional_v<std::optional<T>> = true;

template <typename T>
constexpr inline bool is_vector_like_v = false;

template <typename T, std::size_t N>
constexpr inline bool is_vector_like_v<std::array<T, N>> = true;

template <typename T>
constexpr inline bool is_vector_like_v<std::vector<T>> = true;

}  // namespace clice::impl

namespace clice {

using nlohmann::json;

template <typename Object>
auto serialize(const Object& object) {
    if constexpr(std::is_fundamental_v<Object> || std::is_same_v<Object, std::string_view> ||
                 std::is_same_v<Object, std::string>) {
        return json(object);
    } else if constexpr(impl::is_vector_like_v<Object>) {
        return json(object);
    } else if constexpr(std::is_enum_v<Object>) {
        return static_cast<std::underlying_type_t<Object>>(object);
    } else {
        json json;
        for_each(object, [&json](std::string_view name, auto& member) {
            using Member = std::remove_cvref_t<decltype(member)>;
            if constexpr(impl::is_optional_v<Member>) {
                if(member.has_value()) {
                    json.emplace(name, serialize(member.value()));
                }
            } else {
                json.emplace(name, serialize(member));
            }
        });
        return std::move(json);
    }
}

template <typename Object>
Object deserialize(const json& json) {
    if constexpr(std::is_fundamental_v<Object> || std::is_same_v<Object, std::string_view> ||
                 std::is_same_v<Object, std::string>) {
        return json.get<Object>();
    } else if constexpr(impl::is_vector_like_v<Object>) {
        return json.get<Object>();
    } else if constexpr(std::is_enum_v<Object>) {
        return static_cast<Object>(json.get<std::underlying_type_t<Object>>());
    } else {
        Object object;
        for_each(object, [&json](std::string_view name, auto& member) {
            using Member = std::remove_cvref_t<decltype(member)>;
            if constexpr(impl::is_optional_v<Member>) {
                if(json.contains(name)) {
                    member = deserialize<Member>(json[name]);
                }
            } else {
                member = deserialize<Member>(json[name]);
            }
        });
        return std::move(object);
    }
}

}  // namespace clice

