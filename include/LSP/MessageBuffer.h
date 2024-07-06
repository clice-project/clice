#include <vector>
#include <string_view>

namespace clice {

class MessageBuffer {
    std::vector<char> buffer;
    std::size_t max = 0;

public:
    void write(std::string_view message) { buffer.insert(buffer.end(), message.begin(), message.end()); }

    std::string_view read() {
        std::string_view view = std::string_view(buffer.data(), buffer.size());
        auto start = view.find("Content-Length: ") + 16;
        auto end = view.find("\r\n\r\n");

        if(start != std::string_view::npos || end != std::string_view::npos) {
            std::size_t length = std::stoul(std::string(view.substr(start, end - start)));
            if(view.size() >= length + end + 4) {
                this->max = length + end + 4;
                return view.substr(end + 4, length);
            }
        }

        return {};
    }

    void clear() {
        if(max != 0) {
            buffer.erase(buffer.begin(), buffer.begin() + max);
            max = 0;
        }
    }
};

}  // namespace clice
