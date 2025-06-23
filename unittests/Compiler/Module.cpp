#include "Test/Test.h"
#include "Compiler/Compilation.h"
#include "llvm/Support/ToolOutputFile.h"

namespace clice::testing {

namespace {

PCMInfo buildPCM(llvm::StringRef file, llvm::StringRef code) {
    llvm::SmallString<128> outPath;
    fs::createUniquePath(llvm::Twine(file) + "%%%%%%.pcm", outPath, true);

    std::string path = file.str();
    std::vector<const char*> arguments = {
        "clang++",
        "-std=c++20",
        "-xc++",
        path.c_str(),
    };

    CompilationParams params;
    params.outPath = outPath;
    params.arguments = arguments;
    params.add_remapped_file(file, code);
    params.add_remapped_file("./test.h", "export int foo2();");

    PCMInfo pcm;
    if(!compile(params, pcm)) {
        llvm::errs() << "Failed to build PCM\n";
        std::abort();
    }

    return pcm;
}

ModuleInfo scan(llvm::StringRef content) {
    std::vector<const char*> arguments = {
        "clang++",
        "-std=c++20",
        "-xc++",
        "main.ixx",
    };

    CompilationParams params;
    params.arguments = arguments;
    params.add_remapped_file("main.ixx", content);
    params.add_remapped_file("./test.h", "export module A");
    auto info = scanModule(params);
    if(!info) {
        llvm::errs() << "Failed to scan module\n";
        std::abort();
    }
    return std::move(*info);
}

TEST(Module, Scan) {
    /// Simple case.
    const char* content = R"(
export module A;
import B;    
    )";
    auto info = scan(content);
    ASSERT_EQ(info.isInterfaceUnit, true);
    ASSERT_EQ(info.name, "A");
    ASSERT_EQ(info.mods.size(), 1);
    ASSERT_EQ(info.mods[0], "B");

    /// With global module fragment and private module fragment.
    content = R"(
module;
#include <iostream>
export module A;
import B;    
import C;
module : private;
)";
    info = scan(content);
    ASSERT_EQ(info.isInterfaceUnit, true);
    ASSERT_EQ(info.name, "A");
    ASSERT_EQ(info.mods.size(), 2);
    ASSERT_EQ(info.mods[0], "B");
    ASSERT_EQ(info.mods[1], "C");

    /// With module partition.
    content = R"(
module;
#include <iostream>
export module A:B;
import B;    
import C;
module : private;
)";
    info = scan(content);
    ASSERT_EQ(info.isInterfaceUnit, true);
    ASSERT_EQ(info.name, "A:B");
    ASSERT_EQ(info.mods.size(), 2);
    ASSERT_EQ(info.mods[0], "B");
    ASSERT_EQ(info.mods[1], "C");

    content = R"(
module A;
import B;    
import C;
)";
    info = scan(content);
    ASSERT_EQ(info.isInterfaceUnit, false);
    ASSERT_EQ(info.name, "A");
    ASSERT_EQ(info.mods.size(), 2);
    ASSERT_EQ(info.mods[0], "B");
    ASSERT_EQ(info.mods[1], "C");
}

TEST(Module, Normal) {
    const char* content = R"(
export module A;
)";
    auto pcm = buildPCM("A.ixx", content);
    // ASSERT_EQ(pcm.isInterfaceUnit, true);
    // ASSERT_EQ(pcm.name, "A");
    // ASSERT_EQ(pcm.mods.size(), 0);
}

// TEST(Module, ScanModuleName) {
//     CompilationParams params;
//
//     /// Test module name not in condition directive.
//     params.content = "export module A;";
//     ASSERT_EQ(scanModuleName(params), "A");
//
//     params.content = "export module A.B.C.D;";
//     ASSERT_EQ(scanModuleName(params), "A.B.C.D");
//
//     params.content = "export module A:B;";
//     ASSERT_EQ(scanModuleName(params), "A:B");
//
//     params.content = R"(
// module;
// #ifdef TEST
// #include <iostream>
// #endif
// export module A;
//)";
//     ASSERT_EQ(scanModuleName(params), "A");
//
//     /// Test non-module interface unit.
//     params.content = "module A;";
//     ASSERT_EQ(scanModuleName(params), "");
//
//     params.content = "";
//     ASSERT_EQ(scanModuleName(params), "");
//
//     /// Test module name in condition directive.
//     params.content = R"(
// #ifdef TEST
// export module A;
// #else
// export module B;
// #endif
//)";
//     params.srcPath = "main.cppm";
//     params.command = "clang++ -std=c++20 -x c++ main.cppm -DTEST";
//     ASSERT_EQ(scanModuleName(params), "A");
//
//     params.command = "clang++ -std=c++20 -x c++ main.cppm";
//     ASSERT_EQ(scanModuleName(params), "B");
// }

}  // namespace

}  // namespace clice::testing
