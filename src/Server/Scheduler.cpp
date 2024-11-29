#include "Server/Scheduler.h"
#include "Server/Server.h"

namespace clice {

void PCH::apply(Compiler& compiler) const {
    bool endAtStart = preamble.ends_with('@');
    auto size = preamble.size() - endAtStart;

    if(size != 0) {
        compiler.applyPCH(path, size, endAtStart);
    }
}

struct Tracer {
    std::chrono::system_clock::time_point start = std::chrono::system_clock::now();

    auto duration() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start);
    }
};

async::promise<void> Scheduler::updatePCH(llvm::StringRef filepath,
                                          llvm::StringRef content,
                                          llvm::ArrayRef<const char*> args) {

    std::string outpath = "/home/ykiko/C++/clice2/build/cache/xxx.pch";

    auto [iter, success] = pchs.try_emplace(filepath);
    if(success || iter->second.needUpdate(content)) {
        log::info("Start building PCH for {0}", filepath.str());

        Tracer tracer;
        clang::PreambleBounds bounds = {0, 0};
        co_await async::schedule_task([&] {
            Compiler compiler(filepath, content, args);
            bounds = clang::Lexer::ComputePreamble(content, {}, false);
            if(bounds.Size != 0) {
                compiler.generatePCH(outpath, bounds.Size, bounds.PreambleEndsAtStartOfLine);
            }
        });

        auto preamble = content.substr(0, bounds.Size).str();
        if(bounds.PreambleEndsAtStartOfLine) {
            preamble.append("@");
        }

        pchs[filepath] = PCH{
            .path = outpath,
            .preamble = preamble,
            .deps = {},
        };

        log::info("PCH for {0} is up-to-date, elapsed {1}ms",
                  filepath.str(),
                  tracer.duration().count());
    } else {
        log::info("Reuse PCH from {0}", filepath.str());
    }
    co_return;
}

async::promise<void> Scheduler::buildAST(llvm::StringRef filepath, llvm::StringRef content) {
    llvm::SmallString<128> path = filepath;

    auto [iter, success] = files.try_emplace(filepath);
    if(!success && !iter->second.isIdle) {
        /// If the file is already existed and is building, append the task to the waiting list.
        co_await async::suspend([&](auto handle) {
            iter->second.waitings.emplace_back(Task{
                .isBuild = true,
                .waiting = handle,
            });
        });
    }

    files[filepath].isIdle = false;

    /// FIXME: lookup from CDB file and adjust and remove unnecessary arguments.1
    llvm::SmallVector<const char*> args = {
        "clang++",
        "-std=c++20",
        path.c_str(),
        "-resource-dir",
        "/home/ykiko/C++/clice2/build/lib/clang/20",
    };

    /// through arguments to judge is it a module.
    bool isModule = false;
    co_await (isModule ? updatePCM() : updatePCH(filepath, content, args));

    Tracer tracer;
    log::info("Start building AST for {0}", filepath.str());

    auto task = [&path, &content, &args, pch = pchs.at(filepath)] {
        /// FIXME: We cannot use reference capture the `pch` here, beacuse the reference may be
        /// Invalid Because other changed the `pchs` map. We also cannot to retrieve the `pch` from
        /// the `pchs` map in this task, beacuse it is called in thread pool which will result in
        /// data race. So temporarily copy the `pch` here. There must be a better way to solve this
        /// problem.
        std::unique_ptr<Compiler> compiler = std::make_unique<Compiler>(path, content, args);
        pch.apply(*compiler);
        compiler->buildAST();
        return compiler;
    };

    auto compiler = co_await async::schedule_task(std::move(task));

    auto& file = files[path];
    file.compiler = std::move(compiler);

    log::info("Build AST successfully for {0}, elapsed {1}ms",
              filepath.str(),
              tracer.duration().count());

    if(!file.waitings.empty()) {
        auto task = std::move(file.waitings.front());
        async::schedule(task.waiting);
        file.waitings.pop_front();
    }

    file.isIdle = true;
}

async::promise<void> Scheduler::add(llvm::StringRef path, llvm::StringRef content) {
    co_await buildAST(path, content);
    co_return;
}

async::promise<void> Scheduler::update(llvm::StringRef path, llvm::StringRef content) {
    co_await buildAST(path, content);
    co_return;
}

async::promise<void> Scheduler::save(llvm::StringRef path) {
    co_return;
}

async::promise<void> Scheduler::close(llvm::StringRef path) {
    co_return;
}

}  // namespace clice
