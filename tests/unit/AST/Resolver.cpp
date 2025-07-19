#include "Test/CTest.h"
#include "clang/AST/RecursiveASTVisitor.h"

namespace clice::testing {

namespace {

struct InputFinder : clang::RecursiveASTVisitor<InputFinder> {
    CompilationUnit& unit;
    clang::QualType input;
    clang::QualType expect;

    using Base = clang::RecursiveASTVisitor<InputFinder>;

    InputFinder(CompilationUnit& unit) : unit(unit) {}

    bool TraverseDecl(clang::Decl* decl) {
        if(decl && (llvm::isa<clang::TranslationUnitDecl>(decl) ||
                    unit.file_id(decl->getLocation()) == unit.interested_file())) {
            return Base::TraverseDecl(decl);
        }

        return true;
    }

    bool VisitTypedefNameDecl(const clang::TypedefNameDecl* decl) {
        if(decl->getName() == "input") {
            input = decl->getUnderlyingType();
        }

        if(decl->getName() == "expect") {
            expect = decl->getUnderlyingType();
        }

        return true;
    }
};

struct TemplateResolver : TestFixture {
    void run(llvm::StringRef code, LocationChain chain = LocationChain()) {
        addMain("main.cpp", code);
        compile();

        InputFinder finder(*unit);
        finder.TraverseAST(unit->context());

        auto input = unit->resolver().resolve(finder.input);
        auto expect = finder.expect;

        EXPECT_EQ(input.isNull(), false, chain);
        EXPECT_EQ(expect.isNull(), false, chain);
        EXPECT_EQ(input.getCanonicalType(), expect.getCanonicalType(), chain);
    }
};

TEST_F(TemplateResolver, TypeParameterType) {
    run(R"cpp(
template <typename T>
struct A {
    using type = T;
};

template <typename X>
struct test {
    using input = typename A<X>::type;
    using expect = X;
};
)cpp");
}

TEST_F(TemplateResolver, SingleLevel) {
    run(R"cpp(
template <typename... Ts>
struct type_list {};

template <typename T>
struct A {
    using type = type_list<T>;
};

template <typename X>
struct test {
    using input = typename A<X>::type;
    using expect = type_list<X>;
};
)cpp");
}

TEST_F(TemplateResolver, SingleLevelNotDependent) {
    run(R"cpp(
template <typename T>
struct A {
    using type = int;
};

template <typename X>
struct test {
    using input = typename A<X>::type;
    using expect = int;
};
)cpp");
}

TEST_F(TemplateResolver, MultiLevel) {
    run(R"cpp(
template <typename... Ts>
struct type_list {};

template <typename T1>
struct A {
    using type = type_list<T1>;
};

template <typename T2>
struct B {
    using type = typename A<T2>::type;
};

template <typename T3>
struct C {
    using type = typename B<T3>::type;
};

template <typename X>
struct test {
    using input = typename C<X>::type;
    using expect = type_list<X>;
};
)cpp");
}

TEST_F(TemplateResolver, MultiLevelNotDependent) {
    run(R"cpp(
template <typename T1>
struct A {
    using type = int;
};

template <typename T2>
struct B {
    using type = typename A<T2>::type;
};

template <typename T3>
struct C {
    using type = typename B<T3>::type;
};

template <typename X>
struct test {
    using input = typename C<X>::type;
    using expect = int;
};
)cpp");
}

TEST_F(TemplateResolver, ArgumentDependent) {
    run(R"cpp(
template <typename... Ts>
struct type_list {};

template <typename T1>
struct A {
    using type = T1;
};

template <typename T2>
struct B {
    using type = type_list<T2>;
};

template <typename X>
struct test {
    using input = typename B<typename A<X>::type>::type;
    using expect = type_list<X>;
};
)cpp");
}

TEST_F(TemplateResolver, AliasArgument) {
    run(R"cpp(
template <typename... Ts>
struct type_list {};

template <typename T1>
struct A {
    using type = T1;
};

template <typename T2>
struct B {
    using base = A<T2>;
    using type = type_list<typename base::type>;
};

template <typename X>
struct test {
    using input = typename B<X>::type;
    using expect = type_list<X>;
};
)cpp");
}

TEST_F(TemplateResolver, AliasDependent) {
    run(R"cpp(
template <typename... Ts>
struct type_list {};

template <typename T1>
struct A {
    using type = type_list<T1>;
};

template <typename T2>
struct B {
    using base = A<T2>;
    using type = typename base::type;
};

template <typename X>
struct test {
    using input = typename B<X>::type;
    using expect = type_list<X>;
};
)cpp");
}

TEST_F(TemplateResolver, AliasTemplate) {
    run(R"cpp(
template <typename... Ts>
struct type_list {};

template <typename T1, typename U1>
struct A {
    using type = type_list<T1, U1>;
};

template <typename T2>
struct B {
    template <typename U2>
    using type = typename A<T2, U2>::type;
};

template <typename X, typename Y>
struct test {
    using input = typename B<X>::template type<Y>;
    using expect = type_list<X, Y>;
};
)cpp");
}

TEST_F(TemplateResolver, BaseDependent) {
    run(R"cpp(
template <typename... Ts>
struct type_list {};

template <typename T1>
struct A {
    using type = type_list<T1>;
};

template <typename U2>
struct B : A<U2> {};

template <typename X>
struct test {
    using input = typename B<X>::type;
    using expect = type_list<X>;
};
)cpp");
}

TEST_F(TemplateResolver, MultiNested) {
    run(R"cpp(
template <typename... Ts>
struct type_list {};

template <typename T1>
struct A {
    using self = A<T1>;
    using type = type_list<T1>;
};

template <typename X>
struct test {
    using input = typename A<X>::self::self::self::self::self::type;
    using expect = type_list<X>;
};
)cpp");
}

TEST_F(TemplateResolver, OuterDependentMemberClass) {
    run(R"cpp(
template <typename... Ts>
struct type_list {};

template <typename T1>
struct A {
    template <typename T2>
    struct B {
        template <typename T3>
        struct C {
            using type = type_list<T1, T2, T3>;
        };
    };
};

template <typename X, typename Y, typename Z>
struct test {
    using input = typename A<X>::template B<Y>::template C<Z>::type;
    using expect = type_list<X, Y, Z>;
};
)cpp");
}

TEST_F(TemplateResolver, InnerDependentMemberClass) {
    run(R"cpp(
template <typename... Ts>
struct type_list {};

template <typename T>
struct test {
    template <int N, typename U>
    struct B {
        using type = type_list<U, T>;
    };

    using input = typename B<1, T>::type;
    using expect = type_list<T, T>;
};
)cpp");
}

TEST_F(TemplateResolver, InnerDependentPartialMemberClass) {
    run(R"cpp(
template <typename... Ts>
struct type_list {};

template <typename T, typename U>
struct test {};

template <typename T>
struct test<T, T> {
    template <int N, typename U>
    struct A {
        using type = type_list<U, T>;
    };

    using input = typename A<1, T>::type;
    using expect = type_list<T, T>;
};
)cpp");
}

TEST_F(TemplateResolver, PartialSpecialization) {
    run(R"cpp(
template <typename... Ts>
struct type_list {};

template <typename T1>
struct A {};

template <typename U2>
struct B {};

template <typename U2, template <typename...> typename HKT>
struct B<HKT<U2>> {
    using type = type_list<U2>;
};

template <typename X>
struct test {
    using input = typename B<A<X>>::type;
    using expect = type_list<X>;
};
)cpp");
}

TEST_F(TemplateResolver, PartialDefaultArgument) {
    run(R"cpp(
template <typename T, typename U = T>
struct X {};

template <typename T>
struct X<T, T> {
    using type = T;
};

template <typename T>
struct test {
    using input = typename X<T>::type;
    using expect = T;
};
)cpp");
}

TEST_F(TemplateResolver, DefaultArgument) {
    run(R"cpp(
template <typename... Ts>
struct type_list {};

template <typename T1>
struct A {
    using type = type_list<T1>;
};

template <typename U1, typename U2 = A<U1>>
struct B {
    using type = typename U2::type;
};

template <typename X>
struct test {
    using input = typename B<X>::type;
    using expect = type_list<X>;
};
)cpp");
}

TEST_F(TemplateResolver, PackExpansion) {
    run(R"cpp(
template <typename... Ts>
struct type_list {};

template <typename U, typename... Us>
struct X {
    using type = type_list<Us...>;
};

template <typename... Ts>
struct test {
    using input = typename X<int, Ts...>::type;
    using expect = type_list<Ts...>;
};
)cpp");
}

TEST_F(TemplateResolver, BasePackExpansion) {
    run(R"cpp(
template <typename... Ts>
struct type_list {};

template <typename U, typename... Us>
struct X {
    using type = type_list<Us...>;
};

template <typename... Us>
struct Y : X<int, Us...> {};

template <typename... Ts>
struct test {
    using input = typename Y<Ts...>::type;
    using expect = type_list<Ts...>;
};
)cpp");
}

/// FIXME: headers not found
///
/// TEST_F(TemplateResolver, Standard) {
///     run(R"cpp(
/// #include <vector>
///
/// template <typename T>
/// struct test {
///     using input = typename std::vector<T>::reference;
///     using expect = T&;
/// };
/// )cpp");
/// }

}  // namespace

}  // namespace clice::testing

