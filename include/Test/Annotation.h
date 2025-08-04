#pragma once

#include "AST/SourceCode.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringMap.h"

namespace clice::testing {

struct AnnotatedSource {
    std::string content;
    llvm::StringMap<std::uint32_t> offsets;
    llvm::StringMap<LocalSourceRange> ranges;

    /// Point Annotation:
    /// - $(key): Marks a single point.
    ///
    /// Range Annotation:
    /// - @key[...content...]: Marks a range.
    ///
    /// A range annotation for 'key' creates both a `ranges["key"]` and an `offsets["key"]`
    /// (pointing to the start).
    static AnnotatedSource from(llvm::StringRef content) {
        std::string source;
        source.reserve(content.size());

        llvm::StringMap<std::uint32_t> offsets;
        llvm::StringMap<LocalSourceRange> ranges;

        std::uint32_t offset = 0;
        for(uint32_t i = 0; i < content.size();) {
            char c = content[i];

            /// Handle Range: @key[...]
            if(c == '@') {
                /// Skip '@'
                i += 1;

                const char open_bracket = '[';
                const char close_bracket = ']';

                llvm::StringRef key = content.substr(i).take_until(
                    [&](char c) { return isspace(c) || c == open_bracket; });
                i += key.size();

                while(i < content.size() && isspace(content[i])) {
                    i++;
                }

                assert(i < content.size() && content[i] == open_bracket &&
                       "Expect @key[...] for ranges. e.g., @my_range[int x=1;]");
                i += 1;  // Skip '['

                std::uint32_t range_start = offset;
                int bracket_level = 1;

                while(i < content.size() && bracket_level > 0) {
                    char inner_c = content[i];
                    if(inner_c == open_bracket)
                        bracket_level++;
                    else if(inner_c == close_bracket)
                        bracket_level--;

                    if(bracket_level > 0) {
                        source += inner_c;
                        offset += 1;
                        i += 1;
                    } else {
                        i += 1;  // Skip the final ']'
                    }
                }
                offsets.try_emplace(key, range_start);
                ranges.try_emplace(key, LocalSourceRange{range_start, offset});
                continue;

            }

            /// Handle Point: $(key)
            else if(c == '$') {
                /// Skip '$'
                i += 1;
                assert(i < content.size() && content[i] == '(' && "expect $(key)");

                /// Skip '('
                i += 1;

                /// Skip key.
                llvm::StringRef key =
                    content.substr(i).take_until([&](char c) { return c == ')'; });
                i += key.size();
                assert(i < content.size() && content[i] == ')' && "unclosed $(key)");

                // Skip ')'
                i += 1;

                offsets.try_emplace(key, offset);
                continue;
            }

            source += c;
            offset += 1;
            i += 1;
        }

        return AnnotatedSource{
            std::move(source),
            std::move(offsets),
            std::move(ranges),
        };
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
