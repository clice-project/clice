#include <Clang/Preamble.h>
#include <Support/Logger.h>
#include <Support/Filesystem.h>

namespace clice {

// Preamble
//     Preamble::build(std::string_view path, std::string_view context, const CompilerInvocation& invocation)
//     { auto buffer = llvm::MemoryBuffer::getMemBuffer(context, path, false);
//
//     // compute preamble bounds, if MaxLines set to false(0), it means not to limit the number of lines
//     auto bounds = clang::ComputePreambleBounds({}, *buffer, false);
//
//     clang::DiagnosticsEngine* engine;
//     auto VFS = llvm::vfs::getRealFileSystem();
//     auto dir = fs::path(path).parent_path();
//     if(auto error = VFS->setCurrentWorkingDirectory(dir.string())) {
//         logger::error("failed to set current working directory: {}", error.message());
//     } else {
//         engine = new clang::DiagnosticsEngine(new clang::DiagnosticIDs(), new clang::DiagnosticOptions());
//         engine->setClient(new clang::TextDiagnosticPrinter(llvm::errs(), &engine->getDiagnosticOptions()));
//     }
//
//     // if store the preamble in memory, if not, store it in a file(storagePath)
//     bool storeInMemory = true;
//     llvm::StringRef storagePath = "";
//
//     // use to collect information in the process of building preamble, such as include files and macros
//     // TODO: inherit from clang::PreambleCallbacks and collect the information
//     clang::PreambleCallbacks callbacks = {};
//
//     auto result = clang::PrecompiledPreamble::Build(invocation,
//                                                     buffer.get(),
//                                                     bounds,
//                                                     *engine,
//                                                     VFS,
//                                                     std::make_shared<clang::PCHContainerOperations>(),
//                                                     storeInMemory,
//                                                     storagePath,
//                                                     callbacks);
//     if(result) {
//         logger::info("preamble built successfully");
//         return Preamble{&result.get()};
//     } else {
//         logger::error("failed to build preamble: {}", result.getError().message());
//     }
// }

}  // namespace clice
