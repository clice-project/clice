#include <chrono>

namespace clice {

namespace chrono {

using seconds = std::chrono::seconds;
using milliseconds = std::chrono::milliseconds;
using microseconds = std::chrono::microseconds;

};  // namespace chrono

template <typename T = chrono::seconds>
    requires std::is_same_v<T, std::chrono::duration<typename T::rep, typename T::period>>
class Timer {
public:
    Timer() = default;
    ~Timer() = default;

    Timer(const Timer&) = delete;
    Timer& operator= (const Timer&) = delete;

    /// Start the timer.
    void start() {
        start_time = std::chrono::high_resolution_clock::now();
    }

    /// Stop the timer.
    void stop() {
        end_time = std::chrono::high_resolution_clock::now();
    }

    double getWallTime() {
        return std::chrono::duration_cast<T>(end_time - start_time).count();
    }

private:
    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::high_resolution_clock::time_point end_time;
};

template <typename T>
class TimerRegion {
public:
    explicit TimerRegion(Timer<T>& timer) : timer(timer) {
        timer.start();
    }

    TimerRegion(const TimerRegion&) = delete;
    TimerRegion& operator= (const TimerRegion&) = delete;
    TimerRegion(TimerRegion&&) = delete;

    ~TimerRegion() {
        timer.get().stop();
    }

private:
    std::reference_wrapper<Timer<T>> timer;
};

template <typename T>
TimerRegion(Timer<T>) -> TimerRegion<T>;
};  // namespace clice
