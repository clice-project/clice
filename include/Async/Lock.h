#pragma once

#include "Event.h"

namespace clice::async {

namespace awaiter {

struct lock {
    llvm::SmallVectorImpl<promise_base*>& awaiters;

    bool await_ready() const noexcept {
        return false;
    }

    template <typename Promise>
    void await_suspend(std::coroutine_handle<Promise> handle) const noexcept {
        awaiters.emplace_back(&handle.promise());
    }

    void await_resume() const noexcept {}
};

}  // namespace awaiter

class Lock {
    friend class guard;

public:
    Lock() = default;

    class Guard {
    public:
        Guard(Lock* lock) : lock(lock) {}

        Guard(Guard&& other) : lock(other.lock) {
            other.lock = nullptr;
        }

        ~Guard() {
            if(!lock) {
                return;
            }

            lock->locked = false;
            if(!lock->awaiters.empty()) {
                lock->awaiters.front()->schedule();
                lock->awaiters.erase(lock->awaiters.begin());
            }
        }

    private:
        Lock* lock;
    };

    /// Try to get the lock. If the lock is locked, the current coroutine will be
    /// suspended and wait for the lock to be released.
    Task<Guard> try_lock() {
        /// Note that this task also may be canceled, we make sure
        /// even cancel, it can resume one task(through destructor).
        Guard guard(this);

        if(locked) {
            co_await awaiter::lock{awaiters};
        }

        locked = true;

        /// Use `std::move` to make sure it will not resume
        /// the awaiter here.
        co_return std::move(guard);
    }

private:
    bool locked = false;
    llvm::SmallVector<promise_base*, 4> awaiters;
};

}  // namespace clice::async
