#include <Index/Pack.h>
#include <Support/Reflection.h>

namespace clice {

namespace {

template <typename T>
constexpr bool is_array_ref_v = false;

template <typename T>
constexpr bool is_array_ref_v<llvm::ArrayRef<T>> = true;

static_assert(std::is_trivially_copyable_v<CSIF>, "CSIF must be trivially copyable");
static_assert(sizeof(std::uint64_t) == sizeof(void*), "std::uint64_t must be the same size as void*");

struct Metadata {
    CSIF csif;
    std::uint64_t arrayOffset;
    std::uint64_t stringOffset;
};

class Encoder {
public:
    void encode(StringRef& string) {
        std::size_t offset = stringData.size();
        stringData.insert(stringData.end(), string.begin(), string.end());
        stringData.push_back('\0');
        // modify pointer to offset
        string = StringRef(reinterpret_cast<char*>(offset), string.size());
    }

    template <typename T>
    void encode(ArrayRef<T>& array) {
        std::size_t offset = arrayData.size();
        arrayData.reserve(arrayData.size() + array.size() * sizeof(T));
        for(auto elem: array) {
            // write the element of the array
            encodeMemberRef(elem);
            char* begin = reinterpret_cast<char*>(&elem);
            arrayData.insert(arrayData.end(), begin, begin + sizeof(T));
        }
        // modify pointer to offset
        array = ArrayRef<T>(reinterpret_cast<T*>(offset), array.size());
    }

    template <typename T>
    void encodeMemberRef(T& data) {
        static_assert(!std::is_const_v<T>);
        if constexpr(clice::refl::Reflectable<T>) {
            refl::foreach(data, [&]<typename Field>(std::string_view, Field& field) {
                if constexpr(is_array_ref_v<Field>) {
                    encode(field);
                }
            });
        } else if constexpr(is_array_ref_v<T>) {
            encode(data);
        }
    }

    std::unique_ptr<char[]> pack(CSIF csif) {
        // calculate size
        std::size_t size = sizeof(Metadata) + arrayData.size() + stringData.size();
        std::unique_ptr<char[]> data(new char[size]);

        // fill metadata
        Metadata metadata{csif};
        encodeMemberRef(metadata.csif);
        metadata.arrayOffset = sizeof(Metadata);
        metadata.stringOffset = sizeof(Metadata) + arrayData.size();

        // write metadata
        std::memcpy(data.get(), &metadata, sizeof(Metadata));

        // write arrayData and stringData
        std::size_t offset = sizeof(Metadata);
        std::memcpy(data.get() + offset, arrayData.data(), arrayData.size());
        offset += arrayData.size();
        std::memcpy(data.get() + offset, stringData.data(), stringData.size());

        return data;
    }

private:
    std::vector<char> arrayData;
    std::vector<char> stringData;
};

class Decoder {
public:
    void decode(StringRef& string) {
        string = StringRef(data + stringOffset, string.size());
    }

    template <typename T>
    void decode(ArrayRef<T>& array) {
        array = ArrayRef<T>(reinterpret_cast<T*>(data + arrayOffset), array.size());
        for(auto& elem: array) {
            decodeMemberRef(const_cast<T&>(elem));
        }
    }

    template <typename T>
    void decodeMemberRef(T& data) {
        static_assert(!std::is_const_v<T>);
        if constexpr(clice::refl::Reflectable<T>) {
            refl::foreach(data, [&]<typename Field>(std::string_view, Field& field) {
                if constexpr(is_array_ref_v<Field>) {
                    decode(field);
                }
            });
        } else if constexpr(is_array_ref_v<T>) {
            decode(data);
        }
    }

    CSIF unpack(char* data) {
        Metadata metadata;
        std::memcpy(&metadata, data, sizeof(Metadata));
        this->data = data;
        this->arrayOffset = metadata.arrayOffset;
        this->stringOffset = metadata.stringOffset;
        decodeMemberRef(metadata.csif);
        return metadata.csif;
    }

private:
    char* data;
    std::size_t arrayOffset;
    std::size_t stringOffset;
};

}  // namespace

std::unique_ptr<char[]> pack(CSIF csif) {
    Encoder encoder;
    return encoder.pack(csif);
}

CSIF unpack(char* data) {
    Decoder decoder;
    return decoder.unpack(data);
}

}  // namespace clice
