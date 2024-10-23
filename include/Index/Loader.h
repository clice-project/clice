#pragma once

#include <Index/Index.h>

namespace clice::index {

class Loader {
public:
    Loader(char* data) : data(data), index(*reinterpret_cast<out::Index*>(data)) {
        // llvm::outs() << "Symbol count: " << index.symbols.length << "\n";
        int x = 1;
    }

    const out::Symbol& locate(in::SymbolID symbolID);

    in::SymbolID locate(in::Location location);

    template <typename T>
    const T& access(out::Ref<T> ref) {
        return *reinterpret_cast<const T*>(data + ref.offset);
    }

    llvm::StringRef make_string(out::StringRef ref) {
        return {data + ref.offset, ref.length};
    }

    template <typename T>
    std::pair<const T*, const T*> make_range(out::ArrayRef<T> array) {
        const T* begin = reinterpret_cast<const T*>(data + array.offset);
        return {begin, begin + array.length};
    }

private:
    char* data;
    const out::Index& index;
};

}  // namespace clice::index
