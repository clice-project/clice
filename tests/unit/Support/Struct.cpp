#include "Test/Test.h"
#include "Support/Struct.h"

namespace clice::testing {

namespace {

struct X {
    int x;
    int y;

    friend constexpr bool operator== (const X& lhs, const X& rhs) noexcept = default;
};

static_assert(std::is_same_v<refl::member_types<X>, type_list<int, int>>);

suite<"Struct"> struct_tests = [] {
    test("FieldName") = [&] {
        static struct X {
            int a;
            int b;
        } x;

        static_assert(refl::impl::member_name<&x.a>() == "a", "Member name mismatch");
        static_assert(refl::impl::member_name<&x.b>() == "b", "Member name mismatch");
        static_assert(refl::member_names<X>() == std::array<std::string_view, 2>{"a", "b"});

        static struct Y {
            X x;
        } y;

        static_assert(refl::impl::member_name<&y.x>() == "x", "Member name mismatch");
        static_assert(refl::impl::member_name<&y.x.a>() == "a", "Member name mismatch");
        static_assert(refl::impl::member_name<&y.x.b>() == "b", "Member name mismatch");
        static_assert(refl::member_names<Y>() == std::array<std::string_view, 1>{"x"});

        struct H {
            X x;
            Y y;
            H() = delete;
        };

        static union Z {
            char dummy;
            H h;

            Z() {};
            ~Z() {};
        } z;

        static_assert(refl::impl::member_name<&z.h.x>() == "x", "Member name mismatch");
        static_assert(refl::impl::member_name<&z.h.y>() == "y", "Member name mismatch");
        static_assert(refl::impl::member_name<&z.h.y.x>() == "x", "Member name mismatch");

        struct M {
            X x;
            Y y;
            H h;
        };

        static_assert(refl::member_names<M>() == std::array<std::string_view, 3>{"x", "y", "h"});
    };

    test("Foreach") = [&] {
        bool x = false, y = false;
        refl::foreach(X{1, 2}, [&](auto name, auto value) {
            if(name == "x") {
                x = true;
                expect(that % value == 1);
            } else if(name == "y") {
                y = true;
                expect(that % value == 2);
            } else {
                expect(false);
            }
        });
        expect(that % x && y);

        X x1 = {1, 2};
        X x2 = {3, 4};
        expect(that % refl::foreach(x1, x2, [](auto& lhs, auto& rhs) { return lhs = rhs; }));
        expect(that % x1.x == 3);
        expect(that % x1.y == 4);
    };

    test("Inheritance") = [&] {
        inherited_struct(Y, X) {
            int z;
        };

        static_assert(std::is_same_v<refl::member_types<Y>, type_list<int, int, int>>);

        bool x = false, y = false, z = false;
        refl::foreach(Y{1, 2, 3}, [&](auto name, auto value) {
            if(name == "x") {
                x = true;
                expect(that % value == 1);
            } else if(name == "y") {
                y = true;
                expect(that % value == 2);
            } else if(name == "z") {
                z = true;
                expect(that % value == 3);
            } else {
                expect(false);
            }
        });
        expect(that % x && y && z);

        Y y1 = {1, 2, 3};
        Y y2 = {4, 5, 6};
        expect(that % refl::foreach(y1, y2, [](auto& lhs, auto& rhs) { return lhs = rhs; }));
        expect(that % y1.x == 4);
        expect(that % y1.y == 5);
        expect(that % y1.z == 6);
    };

    test("TupleLike") = [&] {
        std::pair<int, int> p = {1, 2};

        static_assert(refl::member_names<decltype(p)>() ==
                      std::array<std::string_view, 2>{"0", "1"});

        std::tuple<int, int> t = {1, 2};

        static_assert(refl::member_names<decltype(t)>() ==
                      std::array<std::string_view, 2>{"0", "1"});

        bool x = false, y = false;
        refl::foreach(t, [&](auto name, auto value) {
            if(name == "0") {
                x = true;
                expect(that % value == 1);
            } else if(name == "1") {
                y = true;
                expect(that % value == 2);
            } else {
                expect(false);
            }
        });
        expect(that % x && y);

        std::tuple<int, int> t1 = {1, 2};
        std::tuple<int, int> t2 = {3, 4};

        expect(that % refl::foreach(t1, t2, [](auto& lhs, auto& rhs) { return lhs = rhs; }));
        expect(that % std::get<0>(t1) == 3);
        expect(that % std::get<1>(t1) == 4);
        expect(that % std::get<0>(t2) == 3);
    };
};

}  // namespace

}  // namespace clice::testing

