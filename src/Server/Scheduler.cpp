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

/// Check whether the file has been modified after the given status.
static bool hasModifiedAfter(fs::file_status& src, llvm::StringRef file) {
    fs::file_status status;
    if(auto error = fs::status(file, status)) {
        log::warn("Failed to get status of {0}, because {1}", file, error.message());
        return true;
    }

    return status.getLastModificationTime() > src.getLastModificationTime();
}

async::promise<> Scheduler::updatePCH(llvm::StringRef srcPath, llvm::StringRef content) {
    bool needUpdate = false;

    PCHInfo info;

    /// Check whether the PCH needs to be updated.
    if(auto iter = pchs.find(srcPath); iter == pchs.end()) {
        needUpdate = true;
    } else {
        info = iter->second;
        co_await async::schedule_task([&content, &needUpdate, &info] {
            /// Check whether PCH file exists.
            if(!fs::exists(info.path)) {
                needUpdate = true;
            }

            /// Check whether the content of the PCH is consistent with the source file.
            auto size = info.bounds().Size;
            if(content.substr(0, size) != info.preamble.substr(0, size)) {
                needUpdate = true;
            }

            /// Check whether the dependent files have been modified.
            fs::file_status status;
            if(auto error = fs::status(info.path, status)) {
                log::warn("Failed to get status of {0}, because {1}", info.path, error.message());
                needUpdate = true;
            }

            for(auto& dep: info.deps) {
                if(hasModifiedAfter(status, dep)) {
                    needUpdate = true;
                }
            }
        });
    }

    if(!needUpdate) {
        log::info("PCH for {0} is already up-to-date, reuse it", srcPath);
        co_return;
    }

    CompilationParams params;
    params.content = content;
    params.srcPath = srcPath;
    params.outPath = getPCHOutPath(srcPath);

    /// Build PCH.
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

llvm::Error Scheduler::addModuleDeps(CompilationParams& params,
                                     const ModuleInfo& moduleInfo) const {
    for(auto& mod: moduleInfo.mods) {
        auto iter = pcms.find(mod);

        if(iter == pcms.end()) {
            return error("Cannot find PCM for module {0}", mod);
        }

        /// Add prerequired PCM.
        if(auto error = addModuleDeps(params, iter->second)) {
            return error;
        }

        params.addPCM(iter->second);
    }

    return llvm::Error::success();
}

async::promise<> Scheduler::updatePCM(llvm::StringRef moduleName, class Synchronizer& sync) {
    llvm::StringRef srcPath = sync.map(moduleName);
    if(srcPath.empty()) {
        log::warn("Cannot find source file for module {0}", moduleName);
        co_return;
    }

    /// At first, scan the module to get module name and dependent modules.
    CompilationParams params;
    params.srcPath = srcPath;
    params.command = sync.lookup(srcPath);
    params.outPath = getPCMOutPath(srcPath);

    auto moduleInfo = scanModule(params);
    if(!moduleInfo) {
        log::warn("Build AST for {0} failed, because {1}", srcPath, moduleInfo.takeError());
        co_return;
    }

    /// If the module is an interface unit, we need to update the module map.
    if(moduleInfo->isInterfaceUnit) {
        sync.sync(moduleName, srcPath);
    }

    /// FIXME: If two pcms have same deps, we will check the same deps twice.
    /// Try to skip this by using a set to store deps.

    /// Try to update dependent PCMs.
    for(auto& mod: moduleInfo->mods) {
        co_await updatePCM(mod, sync);
    }

    /// All dependent PCMs are up-to-date, check whether the PCM needs to be updated.
    bool needUpdate = false;

    PCMInfo info;

    if(auto iter = pcms.find(moduleName); iter == pcms.end()) {
        needUpdate = true;
    } else {
        info = iter->second;
        needUpdate = co_await async::schedule_task([&info] {
            /// Check whether PCM file exists.
            if(!fs::exists(info.path)) {
                return true;
            }

            fs::file_status status;
            if(auto error = fs::status(info.path, status)) {
                log::warn("Failed to get status of {0}, because {1}", info.path, error.message());
                return true;
            }

            /// Check whether the source file has been modified.
            if(hasModifiedAfter(status, info.srcPath)) {
                return true;
            }

            /// Check whether the dependent files have been modified.
            for(auto& dep: info.deps) {
                if(hasModifiedAfter(status, dep)) {
                    return true;
                }
            }

            return false;
        });
    }

    if(!needUpdate) {
        log::info("PCM for {0} is already up-to-date, reuse it", srcPath);
        co_return;
    }

    /// Build PCM.
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

async::promise<> Scheduler::updateAST(llvm::StringRef filename,
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
        co_await updatePCH(filename, content);
        params.addPCH(pchs[params.srcPath]);
    } else {
        /// FIXME: it is possible that building PCM parallelly, if they have same deps.
        for(auto& mod: moduleInfo->mods) {
            co_await updatePCM(mod, sync);
        }

        if(auto error = addModuleDeps(params, *moduleInfo)) {
            log::warn("Failed to build PCM for {0}, because {1}", filename, error);
            co_return;
        }
    }

    /// Build AST.
    ASTInfo& info = *files[filename].info;

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

    info.tu()->dump();

    /// Build AST successfully.
    log::info("AST for {0} is up-to-date, elapsed {1}", filename, tracer.duration());
}

void Scheduler::loadCache() {
    llvm::SmallString<128> fileName;
    path::append(fileName, config::frontend().cache_directory, "cache.json");

    auto buffer = llvm::MemoryBuffer::getFile(fileName);
    if(!buffer) {
        log::warn("Failed to load cache from disk, because {0}", buffer.getError().message());
        return;
    }

    auto json = json::parse(buffer.get()->getBuffer());
    if(!json) {
        log::warn("Failed to parse cache from disk, because {0}", json.takeError());
        return;
    }

    auto object = json->getAsObject();
    if(!object) {
        log::warn("Failed to parse cache from disk, because {0}", json.takeError());
        return;
    }

    if(auto pchArray = object->getArray("PCH")) {
        for(auto& value: *pchArray) {
            auto pch = json::deserialize<PCHInfo>(value);
            pchs[pch.srcPath] = std::move(pch);
        }
    }

    if(auto pcmArray = object->getArray("PCM")) {
        for(auto& value: *pcmArray) {
            auto pcm = json::deserialize<PCMInfo>(value);
            pcms[pcm.name] = std::move(pcm);
        }
    }

    log::info("Cache loaded from {0}", fileName);
}

void Scheduler::saveCache() const {
    json::Object result;

    json::Array pchArray;
    for(auto& [name, pch]: pchs) {
        pchArray.emplace_back(json::serialize(pch));
    }
    result.try_emplace("PCH", std::move(pchArray));

    json::Array pcmArray;
    for(auto& [name, pcm]: pcms) {
        pcmArray.emplace_back(json::serialize(pcm));
    }
    result.try_emplace("PCM", std::move(pcmArray));

    llvm::SmallString<128> fileName;
    path::append(fileName, config::frontend().cache_directory, "cache.json");

    std::error_code EC;
    llvm::raw_fd_ostream stream(fileName, EC, llvm::sys::fs::OF_Text);

    if(EC) {
        log::warn("Failed save cache to disk, because {0}", EC.message());
        return;
    }

    stream << json::Value(std::move(result));
    log::info("Cache saved to {0}", fileName);
}

async::promise<> Scheduler::waitForFile(llvm::StringRef filename) {
    File* file = &files[filename];

    if(!file->isIdle) {
        co_await async::suspend([file](auto handle) { file->waiters.push_back(handle); });
    }
}

void Scheduler::scheduleNext(llvm::StringRef filename) {
    auto file = &files[filename];
    if(file->waiters.empty()) {
        file->isIdle = true;
    } else {
        /// If waiters exist, wake up the first waiter.
        auto handle = file->waiters.front();
        file->waiters.pop_front();
        async::schedule(handle);
    }
}

async::promise<> Scheduler::update(llvm::StringRef filename,
                                   llvm::StringRef content,
                                   class Synchronizer& sync) {
    co_await waitForFile(filename);

    /// files may be modified during the action.
    auto file = &files[filename];
    file->isIdle = false;

    /// If the file is idle, execute the action directly.
    assert(file->info && "ASTInfo is required");
    co_await updateAST(filename, content, sync);

    scheduleNext(filename);
}

async::promise<> Scheduler::execute(llvm::StringRef filename,
                                    llvm::unique_function<void(ASTInfo& info)> action) {
    co_await waitForFile(filename);

    /// files may be modified during the action.
    auto file = &files[filename];
    assert(file->info && "ASTInfo is required");

    /// If the file is idle, execute the action directly.
    file->isIdle = false;
    co_await async::schedule_task([&action, file] { action(*file->info); });

    scheduleNext(filename);
}

}  // namespace clice
