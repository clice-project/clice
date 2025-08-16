#include "Test/Test.h"
#include "Async/Async.h"

namespace clice::testing {

namespace {

suite<"Async"> suite = [] {
    test("Event") = [] {
        async::Event event;

        int x = 0;

        auto task1 = [&]() -> async::Task<> {
            expect(that % x == 0);
            co_await event;
            expect(that % x == 1);
            x = 2;
        };

        auto task2 = [&]() -> async::Task<> {
            expect(that % x == 0);
            co_await event;
            expect(that % x == 2);
            x = 3;
        };

        auto main = [&]() -> async::Task<> {
            x = 1;
            event.set();
            co_return;
        };

        async::run(task1(), task2(), main());
    };

    test("EventClear") = [] {
        async::Event event;

        int x = 0;

        auto task1 = [&]() -> async::Task<> {
            expect(that % x == 0);
            co_await event;
            expect(that % x == 1);
            x = 2;
        };

        auto task2 = [&]() -> async::Task<> {
            expect(that % x == 0);
            co_await event;
            expect(that % x == 2);
            x = 3;
        };

        auto main = [&]() -> async::Task<> {
            x = 1;
            event.set();
            co_return;
        };

        async::run(task1(), task2(), main());
    };
};

}  // namespace

}  // namespace clice::testing
