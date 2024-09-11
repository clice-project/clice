#pragma once

#include <llvm/Support/JSON.h>
#include <Support/Reflection.h>
#include <array>

namespace clice::json {

using namespace llvm::json;

template <typename T>
constexpr inline bool is_array_v = false;

template <typename T, std::size_t N>
constexpr inline bool is_array_v<std::array<T, N>> = true;

template <typename T>
constexpr inline bool is_string_v = false;

template <>
constexpr inline bool is_string_v<std::string> = true;

template <>
constexpr inline bool is_string_v<std::string_view> = true;

template <>
constexpr inline bool is_string_v<llvm::StringRef> = true;

template <typename T>
constexpr inline bool is_integral_v =
    std::is_same_v<T, int> || std::is_same_v<T, unsigned> || std::is_same_v<T, long> || std::is_same_v<T, long long>;

template <typename Value>
json::Value serialize(const Value& value) {
    if constexpr(std::is_same_v<Value, bool>) {
        return value;
    } else if constexpr(is_integral_v<Value> || std::is_enum_v<Value>) {
        return static_cast<int64_t>(value);
    } else if constexpr(std::is_floating_point_v<Value>) {
        return static_cast<double>(value);
    } else if constexpr(is_string_v<Value>) {
        return llvm::StringRef(value);
    } else if constexpr(is_array_v<Value>) {
        json::Array array;
        for(const auto& element: value) {
            array.push_back(serialize(element));
        }
        return array;
    } else {
        json::Object object;
        for_each(value, [&](llvm::StringRef name, const auto& field) {
            object.try_emplace(name, serialize(field));
        });
        return object;
    }
}

template <typename T>
T deserialize(const Object& object) {
    T result;
    for_each(result, [&]<typename Value>(llvm::StringRef name, Value& value) {
        if constexpr(is_array_v<Value>) {
            if(const auto* array = object.getArray(name)) {
                for(std::size_t i = 0; i < array->size(); ++i) {
                    value[i] = deserialize<typename Value::value_type>((*array)[i]);
                }
            }
        } else if constexpr(std::is_same_v<Value, bool>) {
            if(auto boolean = object.getBoolean(name)) {
                value = *boolean;
            }
        } else if constexpr(is_integral_v<Value>) {
            if(auto integer = object.getInteger(name)) {
                value = *integer;
            }
        } else if constexpr(is_integral_v<Value>) {
            if(auto floating = object.getNumber(name)) {
                value = *floating;
            }
        } else if constexpr(std::is_same_v<Value, std::string>) {
            if(auto string = object.getString(name)) {
                value = *string;
            }
        } else {
            if(auto subobject = object.getObject(name)) {
                value = deserialize<Value>(*subobject);
            }
        }
    });
    return result;
}

}  // namespace clice::json
