#include "Server/Indexer.h"
#include "Server/IncludeGraph.h"
#include "Support/Logger.h"

namespace clice {

Indexer::Indexer(CompilationDatabase& database, const config::IndexOptions& options) :
    IncludeGraph(options), database(database), options(options) {}

void Indexer::add(std::string file) {
    /// If the file is already indexed, cancel and start a new indexing task.
    if(auto it = tasks.find(file); it != tasks.end()) [[unlikely]] {
        auto& task = it->second;
        assert(!task.done() && "if task is done, it should be removed from the map");
        task.cancel();
        task.dispose();
        task = index(file);
        task.schedule();
        return;
    }

    /// If the size of the tasks list is less than the concurrency, add the file
    /// to the tasks list and start a new indexing task.
    if(tasks.size() != concurrency) {
        auto task = index(file);
        task.schedule();
        tasks.try_emplace(file, std::move(task));
        pending.erase(file);
        return;
    }

    /// Finally, all tasks are running, add the file to the pending list.
    pending.insert(file);
}

void Indexer::remove(std::string file) {
    if(pending.contains(file)) {
        pending.erase(file);
        return;
    }

    if(auto it = tasks.find(file); it != tasks.end()) {
        it->second.cancel();
        tasks.erase(it);

        return;
    }
}

void Indexer::indexAll() {
    for(auto& entry: database) {
        add(entry.first().str());
    }
}

void Indexer::save() {
    auto json = IncludeGraph::dump();
    auto result = fs::write(path::join(options.dir, "index.json"), std::format("{}", json));
    if(result) {
        log::info("Successfully saved index to disk");
    } else {
        log::warn("Failed to save index to disk: {}", result.error());
    }
}

void Indexer::load() {
    auto path = path::join(options.dir, "index.json");
    auto file = fs::read(path);
    if(!file) {
        log::warn("Failed to open index file: {}", file.error());
        return;
    }

    if(auto result = json::parse(file.value())) {
        IncludeGraph::load(*result);
        log::info("Successfully loaded index from disk");
    } else {
        log::warn("Failed to parse index file: {}", result.takeError());
    }
}

async::Task<> Indexer::index(std::string file) {
    assert(!pending.contains(file) && "file should not be in the pending list");
    assert(tasks.contains(file) && "file should not be in the tasks list");

    auto task = IncludeGraph::index(file, database);
    co_await task;

    log::info("index process: [running: {}, pending :{}], finish {}",
              tasks.size() - 1,
              pending.size(),
              file);

    auto it = tasks.find(file);
    assert(it != tasks.end() && "file should be in the tasks list");
    /// We cannot directly erase the task from the map, this will call
    /// its destructor and destroy the coroutine. But we are still
    /// executing the coroutine. So dispose it.
    it->second.dispose();
    tasks.erase(it);

    if(pending.empty()) {
        co_return;
    }

    /// Create a new task for the next file and schedule it.
    file = pending.begin()->first();
    auto next = index(file);
    next.schedule();

    /// Remove the file from the pending list and add it to the tasks list.
    pending.erase(pending.begin());
    tasks.try_emplace(file, std::move(next));
}

async::Task<std::optional<index::FeatureIndex>>
    Indexer::getFeatureIndex(std::string& buffer, llvm::StringRef file) const {
    std::string path;
    if(auto tu = tus.find(file); tu != tus.end()) {
        path = tu->second->indexPath;
    }

    if(auto header = headers.find(file); header != headers.end()) {
        for(auto&& index: header->second->indices) {
            path = index.path;
            break;
        }
    }

    auto content = co_await async::fs::read(path + ".fidx");
    if(!content) {
        co_return std::nullopt;
    }

    buffer = std::move(*content);

    co_return index::FeatureIndex(buffer.data(), buffer.size(), false);
}

async::Task<std::vector<feature::SemanticToken>>
    Indexer::semanticTokens(llvm::StringRef file) const {
    std::vector<feature::SemanticToken> result;

    std::string buffer;
    auto index = co_await getFeatureIndex(buffer, file);
    if(!index) {
        co_return result;
    }

    co_return index->semanticTokens();
}

async::Task<std::vector<feature::FoldingRange>> Indexer::foldingRanges(llvm::StringRef file) const {
    std::vector<feature::FoldingRange> result;

    std::string buffer;
    auto index = co_await getFeatureIndex(buffer, file);
    if(!index) {
        co_return result;
    }

    co_return index->foldingRanges();
}

}  // namespace clice
