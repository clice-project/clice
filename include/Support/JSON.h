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
constexpr inline bool is_integral_v =
    std::is_same_v<T, int> || std::is_same_v<T, unsigned> || std::is_same_v<T, long> || std::is_same_v<T, long long>;

template <typename T>
Object serialize(const T& object) {
    Object result;
    for_each(object, [&]<typename Value>(std::string_view name, Value& value) {
        if constexpr(is_array_v<Value>) {
            Array array;
            for(const auto& element: value) {
                array.push_back(serialize(element));
            }
            result.try_emplace(llvm::StringRef(name), std::move(array));
        } else if constexpr(std::is_constructible_v<json::Value, Value&>) {
            result.try_emplace(llvm::StringRef(name), value);
        } else {
            result.try_emplace(llvm::StringRef(name), serialize(value));
        }
    });
    return result;
}

template <typename T>
T deserialize(const Object& object) {
    T result;
    for_each(result, [&]<typename Value>(std::string_view name, Value& value) {
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
