#include "Server/Indexer2.h"
#include "Server/IncludeGraph.h"
#include "Support/Logger.h"

namespace clice {

Indexer2::Indexer2(CompilationDatabase& database, const config::IndexOptions& options) :
    database(database), options(options) {
    graph = new IncludeGraph(options);
}

Indexer2::~Indexer2() {
    delete graph;
}

void Indexer2::add(std::string file) {
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

void Indexer2::remove(std::string file) {
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

void Indexer2::indexAll() {
    for(auto& entry: database) {
        add(entry.first().str());
    }
}

async::Task<> Indexer2::index(std::string file) {
    assert(!pending.contains(file) && "file should not be in the pending list");
    assert(tasks.contains(file) && "file should not be in the tasks list");

    auto task = graph->index(file, database);
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

}  // namespace clice
