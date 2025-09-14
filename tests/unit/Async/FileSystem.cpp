#include "Test/Test.h"
#include "Async/Async.h"
#include "Support/FileSystem.h"

namespace clice::testing {

namespace {

suite<"Async"> suite = [] {
    test("FileSystemRead") = [] {
        auto path = fs::createTemporaryFile("prefix", "suffix");
        expect(that % path.has_value());

        auto result = fs::write(*path, "hello");
        expect(that % result.has_value());

        auto main = [&] -> async::Task<> {
            auto content = co_await async::fs::read(*path);
            expect(that % content.has_value());
            expect(that % *content == std::string_view("hello"));
        };

        async::run(main());
    };

    test("FileSystemWrite") = [] {
        auto path = fs::createTemporaryFile("prefix", "suffix");
        expect(that % path.has_value());

        auto main = [&] -> async::Task<> {
            char buffer[] = "hello";

            auto result = co_await async::fs::write(*path, buffer, 5);
            expect(that % result.has_value());
        };

        async::run(main());

        auto content = fs::read(*path);
        expect(that % content.has_value());
        expect(that % *content == std::string_view("hello"));
    };
};

}  // namespace

}  // namespace clice::testing
