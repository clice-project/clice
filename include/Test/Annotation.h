#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringMap.h"

namespace clice::testing {

class Annotation {
public:
    Annotation(llvm::StringRef content) {
        std::uint32_t offset = 0;

        for(uint32_t i = 0; i < content.size();) {
            auto c = content[i];

            if(c == '@') {
                /// match @name
                i += 1;
                auto key = content.substr(i).take_until([](char c) { return c == ' '; });
                assert(!offsets.contains(key) && "duplicate key");
                offsets.try_emplace(key, offset);
                continue;
            } else if(c == '$') {
                /// match $(name)
                assert(i + 1 < content.size() && content[i + 1] == '(' && "expect $(name)");
                i += 2;
                auto key = content.substr(i).take_until([](char c) { return c == ')'; });
                i += key.size() + 1;
                assert(!offsets.contains(key) && "duplicate key");
                offsets.try_emplace(key, offset);
                continue;
            }

            offset += 1;
            i += 1;
            m_source.push_back(c);
        }
    }

    std::uint32_t offset(llvm::StringRef name) {
        return offsets[name];
    }

    llvm::StringRef source() {
        return m_source;
    }

private:
    std::string m_source;
    llvm::StringMap<std::uint32_t> offsets;
};

}  // namespace clice::testing
