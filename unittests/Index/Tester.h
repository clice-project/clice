#pragma once

#include "../Test.h"
#include "Annotation.h"

#include "Compiler/Compiler.h"
#include "Support/Support.h"
#include "Index/Binary.h"

namespace clice {

using namespace index;

struct IndexerTester {
    ASTInfo info;
    Annotation annotation;

    IndexerTester(llvm::StringRef source, bool json = false) : annotation(source) {
        std::vector<const char*> args = {
            "clang++",
            "-std=c++20",
            "main.cpp",
            "-resource-dir",
            "/home/ykiko/C++/clice2/build/lib/clang/20",
        };

        CompliationParams params;
        params.path = "main.cpp";
        params.args = args;
        params.content = source;

        auto begin1 = std::chrono::high_resolution_clock::now();
        if(auto info = buildAST(params)) {
            this->info = std::move(*info);
        } else {
            llvm::errs() << "Failed to build AST\n";
            std::terminate();
        }
        auto end1 = std::chrono::high_resolution_clock::now();

        auto begin2 = std::chrono::high_resolution_clock::now();
        auto index = index::index(info);
        auto end2 = std::chrono::high_resolution_clock::now();

        auto begin3 = std::chrono::high_resolution_clock::now();
        auto j = index::toJSON(index);
        auto end3 = std::chrono::high_resolution_clock::now();

        auto begin4 = std::chrono::high_resolution_clock::now();
        auto binary = index::toBinary(index);
        auto end4 = std::chrono::high_resolution_clock::now();

        print("build AST: {}, index: {}, json: {}, binary: {}\n",
              std::chrono::duration_cast<std::chrono::milliseconds>(end1 - begin1),
              std::chrono::duration_cast<std::chrono::milliseconds>(end2 - begin2),
              std::chrono::duration_cast<std::chrono::milliseconds>(end3 - begin3),
              std::chrono::duration_cast<std::chrono::milliseconds>(end4 - begin4));

        std::size_t size = 0;
        for(auto& symbol: index.symbols) {
            size += symbol.relations.size();
        }

        print("files count: {}, symbols count: {}, location count: {}, relation count: {}\n",
              index.files.size(),
              index.symbols.size(),
              index.occurrences.size(),
              size);

        print("binary size: {}MB\n", binary.size() / (1024 * 1024));

        std::error_code error;
        llvm::raw_fd_ostream os("index.json", error);
        if(error) {
            llvm::errs() << error.message() << "\n";
            std::terminate();
        }
        os << j;
    }
};

}  // namespace clice
