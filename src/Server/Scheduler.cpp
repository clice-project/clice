#include "Server/Config.h"
#include "Server/Scheduler.h"
#include "Server/Server.h"

namespace clice {

static std::string getPCHOutPath(llvm::StringRef srcPath) {
    llvm::SmallString<128> outPath = srcPath;
    path::replace_path_prefix(outPath, config::workplace(), config::frontend().cache_directory);
    path::replace_extension(outPath, ".pch");

    if(auto dir = path::parent_path(outPath); !fs::exists(dir)) {
        if(auto error = fs::create_directories(dir)) {
            log::fatal("Failed to create directory {0}, because {1}", dir, error.message());
        }
    }

    return outPath.str().str();
}

static std::string getPCMOutPath(llvm::StringRef srcPath) {
    llvm::SmallString<128> outPath = srcPath;
    path::replace_path_prefix(outPath, config::workplace(), config::frontend().cache_directory);
    path::replace_extension(outPath, ".pcm");

    if(auto dir = path::parent_path(outPath); !fs::exists(dir)) {
        if(auto error = fs::create_directories(dir)) {
            log::fatal("Failed to create directory {0}, because {1}", dir, error.message());
        }
    }

    return outPath.str().str();
}

async::promise<> Scheduler::updatePCH(CompilationParams& params, class Synchronizer& sync) {
    llvm::StringRef srcPath = params.srcPath;

    auto& pch = pchs[srcPath];
    /// FIXME: judge need update here ...
    if(!pch.needUpdate(params.content)) {
        log::info("PCH for {0} is already up-to-date, reuse it", srcPath);
        co_return;
    }

    /// Construct the output path.
    params.outPath = getPCHOutPath(srcPath);

    /// Build PCH.
    PCHInfo info;

    Tracer tracer;
    log::info("Building PCH for {0}", srcPath);

    /// FIXME: consider header context.
    params.computeBounds();

    llvm::Error error = co_await async::schedule_task([&] -> llvm::Error {
        auto result = compile(params, info);
        if(!result) {
            return result.takeError();
        }

        /// FIXME: consider indexing PCH here.

        return llvm::Error::success();
    });

    pchs[srcPath] = std::move(info);

    if(error) {
        log::warn("Failed to build PCH for {0}, because {1}", srcPath, error);
        co_return;
    } else {
        log::info("PCH for {0} is up-to-date, elapsed {1}", srcPath, tracer.duration());
    }
}

llvm::Error Scheduler::addModuleDeps(CompilationParams& params, ModuleInfo& moduleInfo) {
    for(auto& mod: moduleInfo.mods) {
        auto& pcm = pcms[mod];

        /// Add prerequired PCM.
        if(auto error = addModuleDeps(params, pcm)) {
            return error;
        }

        if(pcm.name.empty()) {
            return error("Cannot find PCM for module {0}", mod);
        }

        params.addPCM(pcm);
    }

    return llvm::Error::success();
}

async::promise<> Scheduler::updatePCM(llvm::StringRef moduleName, class Synchronizer& sync) {
    llvm::StringRef srcPath = sync.map(moduleName);
    if(srcPath.empty()) {
        log::warn("Cannot find source file for module {0}", moduleName);
        co_return;
    }

    auto& pcm = pcms[moduleName];
    if(!pcm.needUpdate()) {
        log::info("PCM for {0} is already up-to-date, reuse it", srcPath);
    }

    CompilationParams params;
    params.srcPath = srcPath;
    params.command = sync.lookup(srcPath);
    params.outPath = getPCMOutPath(srcPath);

    auto moduleInfo = scanModule(params);
    if(!moduleInfo) {
        log::warn("Build AST for {0} failed, because {1}", srcPath, moduleInfo.takeError());
        co_return;
    }

    /// Build prerequired PCM.
    for(auto& mod: moduleInfo->mods) {
        co_await updatePCM(mod, sync);
    }

    /// Build PCM.
    PCMInfo info;

    Tracer tracer;
    log::info("Building PCM for {0}", srcPath);

    /// Add deps.
    if(auto error = addModuleDeps(params, *moduleInfo)) {
        log::warn("Failed to build PCM for {0}, because {1}", srcPath, error);
        co_return;
    }

    llvm::Error error = co_await async::schedule_task([&] -> llvm::Error {
        auto result = compile(params, info);
        if(!result) {
            return result.takeError();
        }

        /// FIXME: consider indexing PCH here.

        return llvm::Error::success();
    });

    pcms[moduleName] = std::move(info);

    if(error) {
        log::warn("Failed to build PCM for {0}, because {1}", srcPath, error);
        co_return;
    } else {
        log::info("PCM for {0} is up-to-date, elapsed {1}", srcPath, tracer.duration());
    }
}

async::promise<> Scheduler::update(llvm::StringRef filename,
                                   llvm::StringRef content,
                                   class Synchronizer& sync) {
    CompilationParams params;
    params.content = content;
    params.srcPath = filename;
    params.command = sync.lookup(filename);

    auto moduleInfo = scanModule(params);
    if(!moduleInfo) {
        log::warn("Build AST for {0} failed, because {1}", filename, moduleInfo.takeError());
        co_return;
    }

    if(moduleInfo->name.empty() && moduleInfo->mods.empty()) {
        co_await updatePCH(params, sync);
        params.bounds.reset();
        params.addPCH(pchs[params.srcPath]);
    } else {
        for(auto& mod: moduleInfo->mods) {
            co_await updatePCM(mod, sync);
        }

        if(auto error = addModuleDeps(params, *moduleInfo)) {
            log::warn("Failed to build PCM for {0}, because {1}", filename, error);
            co_return;
        }
    }

    /// Build AST.
    ASTInfo info;

    Tracer tracer;

    log::info("Building AST for {0}", filename);

    co_await async::schedule_task([&] {
        auto result = compile(params);
        if(!result) {
            log::warn("Failed to build AST for {0}, because {1}", filename, result.takeError());
            return;
        }

        info = std::move(*result);
    });

    /// Build AST successfully.

    log::info("AST for {0} is up-to-date, elapsed {1}", filename, tracer.duration());
}

}  // namespace clice
