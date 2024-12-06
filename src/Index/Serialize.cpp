// #include <Index/Serialize.h>
// #include <Support/Support.h>
//
// namespace clice::index {
//
// json::Value toJson(const memory::Index& index) {
//     return json::serialize(index);
// }
//
// namespace {
//
// class Packer {
//
//     template <typename T>
//     struct Section {
//         std::size_t offset;
//         std::size_t count;
//         std::vector<T> data;
//     };
//
//     template <typename... Ts>
//     struct Sections {
//         std::tuple<Section<Ts>...> sections;
//
//         /// FIXME: use more flexible way to initialize the layout.
//         void initLayout(std::size_t& offset, const memory::Index& index) {
//             get<binary::string>().count = index.commands.size();
//             get<binary::File>().count = index.files.size();
//             get<binary::Symbol>().count = index.symbols.size();
//             get<Occurrence>().count = index.occurrences.size();
//
//             for(auto& symbol: index.symbols) {
//                 get<Relation>().count += symbol.relations.size();
//             }
//
//             get<Location>().count = index.locations.size();
//
//             auto foreach = [&]<typename T>() {
//                 auto& section = std::get<Section<T>>(sections);
//                 section.offset = offset;
//                 section.data.reserve(section.count);
//                 offset += section.count * sizeof(T);
//             };
//
//             (foreach.template operator()<Ts>(), ...);
//         }
//
//         template <typename T>
//         auto& get() {
//             return std::get<Section<T>>(sections);
//         }
//     };
//
// public:
//     template <typename In, typename Out>
//     void pack(const In& in, Out& out) {
//         if constexpr(requires { out = in; }) {
//             out = in;
//         } else {
//             support::foreach(in, out, [&]<typename U>(const auto& in, U& out) { pack(in, out);
//             });
//         }
//     }
//
//     template <typename T, typename U>
//     void pack(const std::vector<T>& in, binary::array<U>& out) {
//         auto& section = sections.get<U>();
//         out.size = in.size();
//         out.offset = section.offset + sizeof(U) * section.data.size();
//
//         for(const auto& item: in) {
//             pack(item, section.data.emplace_back());
//         }
//     }
//
//     void pack(const memory::string& in, binary::string& out) {
//         out.size = in.size();
//         out.offset = stringOffset;
//
//         if(in.size() != 0) {
//             stringBuffer.insert(stringBuffer.end(), in.begin(), in.end());
//             stringBuffer.push_back('\0');
//             stringOffset += in.size() + 1;
//         }
//     }
//
//     std::vector<char> pack(const memory::Index& in) {
//         binary::Index out = {};
//         stringOffset = sizeof(binary::Index);
//         sections.initLayout(stringOffset, in);
//
//         pack(in, out);
//
//         std::vector<char> binary;
//         binary.reserve(stringOffset);
//         binary.insert(binary.end(),
//                       reinterpret_cast<char*>(&out),
//                       reinterpret_cast<char*>(&out + 1));
//         std::apply(
//             [&binary](auto&... section) {
//                 (binary.insert(binary.end(),
//                                reinterpret_cast<char*>(section.data.data()),
//                                reinterpret_cast<char*>(section.data.data() +
//                                section.data.size())),
//                  ...);
//             },
//             sections.sections);
//
//         binary.insert(binary.end(), stringBuffer.begin(), stringBuffer.end());
//         return binary;
//     }
//
// private:
//     Sections<binary::string, binary::File, binary::Symbol, Occurrence, Relation, Location>
//     sections; std::size_t stringOffset; std::vector<char> stringBuffer;
// };
//
// }  // namespace
//
// std::vector<char> toBinary(const memory::Index& index) {
//     return Packer().pack(index);
// }
//
// }  // namespace clice::index
//
