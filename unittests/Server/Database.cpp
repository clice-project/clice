#include "Test/Test.h"

#include "Server/Database.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

#include <filesystem>

namespace sfs = std::filesystem;

namespace clice::testing {

namespace {

bool getExecutablePath(char* buffer, size_t size) {
#ifdef _WIN32
    DWORD len = GetModuleFileNameA(NULL, buffer, (DWORD)size);
    if(len == 0 || len >= size) {
        return false;
    }
#else
    ssize_t len = readlink("/proc/self/exe", buffer, size - 1);
    if(len == -1) {
        return false;
    }
#endif
    return true;
}

bool recursiveFindCDB(std::string& result) {
#ifdef _WIN32
    char buffer[MAX_PATH] = {0};
#else
    char buffer[PATH_MAX] = {0};
#endif

    if(!getExecutablePath(buffer, sizeof(buffer))) {
        return false;
    }

    sfs::path current_path = buffer;
    while(true) {
        if(auto cdb = current_path / "compile_commands.json"; sfs::exists(cdb)) {
            result = cdb.string();
            return true;
        }

        if(auto dir = current_path.filename().string(); dir.contains("clice")) {
            result = dir;
            return false;
        }

        current_path = current_path.parent_path();
    }

    return false;
}

TEST(CompilationDatabase, Command) {
    std::string cdbPath;
    if(std::getenv("CI") == nullptr || !recursiveFindCDB(cdbPath)) {
        return;
    }

    auto buffer = llvm::MemoryBuffer::getFile(cdbPath);
    EXPECT_TRUE(bool(buffer));

    auto res = CompilationDatabase::parse(buffer.get()->getBuffer());
    EXPECT_TRUE(res.has_value());
    EXPECT_TRUE(!res->empty());

    for(auto& [file, command]: res.value()) {
        EXPECT_TRUE(!file.empty());
        EXPECT_TRUE(!command.empty());
    }
}

TEST(CompilationDatabase, Module) {}

}  // namespace

}  // namespace clice::testing
