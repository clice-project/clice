#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringMap.h"

namespace clice::testing {

struct AnnotatedSource {
    std::string content;
    llvm::StringMap<std::uint32_t> offsets;

    static AnnotatedSource from(llvm::StringRef content) {
        std::string source;
        llvm::StringMap<std::uint32_t> offsets;

        source.reserve(content.size());

        std::uint32_t offset = 0;
        for(uint32_t i = 0; i < content.size();) {
            auto c = content[i];

            if(c == '@') {
                i += 1;
                auto key = content.substr(i).take_until([](char c) { return c == ' '; });
                offsets.try_emplace(key, offset);
                continue;
            }

            if(c == '$') {
                assert(i + 1 < content.size() && content[i + 1] == '(' && "expect $(name)");
                i += 2;
                auto key = content.substr(i).take_until([](char c) { return c == ')'; });
                i += key.size() + 1;
                offsets.try_emplace(key, offset);
                continue;
            }

            i += 1;
            offset += 1;
            source += c;
        }

        return AnnotatedSource{std::move(source), std::move(offsets)};
    }
};

struct AnnotatedSources {
    /// All sources file in the compilation.
    llvm::StringMap<AnnotatedSource> all_files;

    void add_source(llvm::StringRef file, llvm::StringRef content) {
        all_files.try_emplace(file, AnnotatedSource::from(content));
    }

    /// Add sources to the params, use `#[filename]` to mark
    /// a new file start. For example
    ///
    /// ```cpp
    /// #[test.h]
    /// int foo();
    ///
    /// #[main.cpp]
    /// #include "test.h"
    /// int x = foo();
    /// ```
    void add_sources(llvm::StringRef content) {
        std::string curr_file;
        std::string curr_content;

        /// Save previous file to params.
        auto save_previous_file = [&]() {
            if(curr_file.empty()) {
                return;
            }

            add_source(curr_file, curr_content);
            curr_file.clear();
            curr_content.clear();
        };

        while(!content.empty()) {
            llvm::StringRef line = content.take_front(content.find_first_of("\r\n"));
            content = content.drop_front(line.size());
            if(content.starts_with("\n")) {
                content = content.drop_front(2);
            } else if(content.starts_with("\n")) {
                content = content.drop_front(1);
            }

            if(line.starts_with("#[") && line.ends_with("]")) {
                save_previous_file();
                curr_file = line.slice(2, line.size() - 1).str();
            } else if(!curr_file.empty()) {
                curr_content += line;
                curr_content += '\n';
            }
        }

        save_previous_file();
    }
};

}  // namespace clice::testing
