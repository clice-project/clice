#include "Reflection.h"
#include <iostream>
#include <optional>

namespace clice {

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

template <typename Object>
void print(Object&& object) {
    using T = std::decay_t<Object>;
    if constexpr(std::is_same_v<T, bool>) {
        std::cout << (object ? "true" : "false") << std::endl;
    } else if constexpr(std::is_integral_v<T> || std::is_floating_point_v<T> ||
                        std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>) {
        std::cout << object << std::endl;
    } else if constexpr(std::is_enum_v<T>) {
        std::cout << static_cast<std::underlying_type_t<T>>(object) << std::endl;
    } else if constexpr(is_optional_v<T>) {
        if(object.has_value()) {
            print(object.value());
        } else {
            std::cout << "null" << std::endl;
        }
    } else if constexpr(is_vector_like_v<T>) {
        std::cout << "[" << std::endl;
        for(auto&& value: object) {
            print(value);
        }
        std::cout << "]" << std::endl;
    } else {
        std::cout << "{" << std::endl;
        for_each(object, [&](auto&& name, auto&& value) {
            std::cout << name << ": ";
            print(value);
        });
        std::cout << "}" << std::endl;
    }
}
}  // namespace clice

