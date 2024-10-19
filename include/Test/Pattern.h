#pragma once

/// We use constant expression in AST as the input of test.
/// So that you can easily write test case for source code.
/// Especially for location based test.
/// See `Pattern.h`.

namespace clice::test {

struct Location {
    std::size_t line;
    std::size_t column;
};

template <typename... Ts>
struct Hook {
    consteval Hook() {}

    consteval Hook(Ts... args) {}
};

}  // namespace clice::test
