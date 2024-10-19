#pragma once

#include <gtest/gtest.h>
#include <spdlog/fmt/bundled/color.h>

#include <Test/Pattern.h>
#include <Support/JSON.h>
#include <Support/FileSystem.h>

namespace clice {

std::string test_dir();

template <typename Callback>
inline void foreachFile(std::string name, const Callback& callback) {
    llvm::SmallString<128> path;
    path += test_dir();
    path::append(path, name);
    std::error_code error;
    fs::directory_iterator iter(path, error);
    fs::directory_iterator end;
    while(!error && iter != end) {
        auto file = iter->path();
        auto buffer = llvm::MemoryBuffer::getFile(file);
        if(!buffer) {
            llvm::outs() << "failed to open file: " << buffer.getError().message() << file << "\n";
            // TODO:
        }
        auto content = buffer.get()->getBuffer();
        callback(file, content);
        iter.increment(error);
    }
}

extern llvm::StringMap<bool (*)(clang::APValue*, std::size_t)> hooks;

template <typename T>
T transform(clang::APValue& value) {
    if constexpr(json::is_integral_v<T>) {
        return value.getInt().getExtValue();
    } else {
        T result;
        std::size_t index;
        refl::foreach(result, [&]<typename Field>(llvm::StringRef name, Field& field) {
            field = transform<Field>(value.getStructField(index));
            index += 1;
        });
        return result;
    }
}

template <auto>
struct identity {};

template <typename... Ts, bool (*hook)(Ts...)>
int registerHook(std::string name, test::Hook<Ts...>, identity<hook>) {
    auto wrap = +[](clang::APValue* args, std::size_t count) -> bool {
        return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            return hook(transform<Ts>(args[Is])...);
        }(std::make_index_sequence<sizeof...(Ts)>{});
    };
    hooks.try_emplace(name, wrap);
    llvm::outs() << "size: " << hooks.size() << " address: " << &hooks << "\n";
    return 0;
}

#define REGISTER(Hook, Func) static auto side_effect##Func = registerHook(#Hook, Hook{}, identity<Func>{});

bool exec(clang::ASTContext& context);

}  // namespace clice

