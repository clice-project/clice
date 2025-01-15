#pragma once

#include "Basic/Location.h"

namespace clice {

class Annotation {
public:
    Annotation(llvm::StringRef source) : m_source() {
        m_source.reserve(source.size());

        uint32_t line = 0;
        uint32_t column = 0;

        for(uint32_t i = 0; i < source.size();) {
            auto c = source[i];

            if(c == '@') {
                i += 1;
                auto key = source.substr(i).take_until([](char c) { return c == ' '; });
                assert(!locations.contains(key) && "duplicate key");
                locations.try_emplace(key, line, column);
                continue;
            }

            if(c == '$') {
                assert(i + 1 < source.size() && source[i + 1] == '(' && "expect $(name)");
                i += 2;
                auto key = source.substr(i).take_until([](char c) { return c == ')'; });
                i += key.size() + 1;
                assert(!locations.contains(key) && "duplicate key");
                locations.try_emplace(key, line, column);
                continue;
            }

            if(c == '\n') {
                line += 1;
                column = 0;
            } else {
                column += 1;
            }

            i += 1;
            m_source.push_back(c);
        }
    }

    llvm::StringRef source() const {
        return m_source;
    }

    proto::Position position(llvm::StringRef key) const {
        return locations.lookup(key);
    }

private:
    std::string m_source;
    llvm::StringMap<proto::Position> locations;
};

}  // namespace clice
