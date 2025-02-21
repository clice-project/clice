#include "Index/Index.h"
#include "Index/SymbolIndex.h"
#include "Support/FileSystem.h"
#include "llvm/Support/CommandLine.h"

llvm::cl::opt<std::string> input("input", llvm::cl::desc("The input file path prefix"));
llvm::cl::opt<std::string> output("output", llvm::cl::desc("The output dictionary path"));
llvm::cl::opt<std::string> format("format", llvm::cl::desc("The format of index"));

using namespace clice;

int main(int argc, char** argv) {
    llvm::cl::ParseCommandLineOptions(argc, argv, "clice test\n");

    if(input.empty()) {
        llvm::errs() << "Input file path is required\n";
        return 1;
    }

    if(output.empty()) {
        llvm::errs() << "Output dictionary path is required\n";
        return 1;
    }

    if(format.empty()) {
        llvm::errs() << "Format of index is required, symbol or feature";
        return 1;
    }

    auto dir = path::parent_path(input);
    auto suffix = format == "symbol" ? ".sidx" : ".fidx";

    std::vector<std::string> files;

    std::error_code error;
    auto iter = fs::directory_iterator(dir, error);
    while(iter != fs::directory_iterator()) {
        auto entry = *iter;
        llvm::StringRef path = entry.path();
        if(path.starts_with(input) && path.ends_with(suffix)) {
            files.emplace_back(path);
        }

        iter.increment(error);
        if(error) {
            llvm::errs() << "Failed to iterate directory: " << error.message() << "\n";
            return 1;
        }
    }

    for(auto& file: files) {
        auto content = fs::read(file);
        if(!content) {
            llvm::errs() << "Failed to read file: " << file << "\n";
            continue;
        }

        if(format == "symbol") {
            index::SymbolIndex index(content->data(), content->size(), false);
            auto json = index.toJSON();

            llvm::SmallString<128> path;
            path += file;

            path::replace_path_prefix(path, dir, output);
            path::replace_extension(path, ".json");

            auto result = fs::write(path, std::format("{}", json));
            if(!result) {
                llvm::errs() << "Failed to write file: " << path << "\n";
            }
        }
    }

    return 0;
}
