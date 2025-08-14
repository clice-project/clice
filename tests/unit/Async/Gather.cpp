#include "Test/Test.h"
#include "Async/Async.h"

namespace clice::testing {

namespace {

suite<"Async"> suite = [] {
    test("GatherPack") = [] {
        int x = 0;

        auto task_gen = [&]() -> async::Task<int> {
            co_await async::sleep(100);
            x += 1;
            co_return x;
        };

        auto [a, b, c] = async::run(task_gen(), task_gen(), task_gen());

        expect(that % a == 1);
        expect(that % b == 2);
        expect(that % c == 3);
    };

    test("GatherRange") = [] {
        std::vector<int> args;
        for(int i = 0; i < 30; ++i) {
            args.push_back(i);
        }

        std::vector<int> results;

        auto task_gen = [&](int x) -> async::Task<bool> {
            co_await async::sleep(10);
            results.push_back(x);
            co_return true;
        };

        auto core = async::gather(args, task_gen);
        async::run(core);

        expect(that % args == results);
        expect(that % core.result() == true);
    };

    test("GatherCancel") = [] {
        std::vector<int> args;
        for(int i = 0; i < 30; ++i) {
            args.push_back(i);
        }

        std::vector<int> results;

        auto task_gen = [&](int x) -> async::Task<bool> {
            co_await async::sleep(10);
            results.push_back(x);
            co_return false;
        };

        auto core = async::gather(args, task_gen);
        async::run(core);

        expect(that % results.size() == 1);
        expect(that % core.result() == false);
    };
};

}  // namespace

}  // namespace clice::testing
