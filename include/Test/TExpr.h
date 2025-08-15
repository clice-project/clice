#pragma once

#include <string>
#include <format>
#include <algorithm>
#include <functional>

namespace clice {

struct expr_tag {};

template <typename Expr>
decltype(auto) compute(const Expr& expr) {
    if constexpr(requires { typename Expr::expr_tag; }) {
        return expr();
    } else {
        return expr;
    }
}

template <typename Expr>
std::string fmt(const Expr& expr) {
    if constexpr(requires { typename Expr::expr_tag; }) {
        return expr.format();
    } else {
        return std::format("{}", expr);
    }
}

#define BINARY_OP(name, op)                                                                        \
    template <typename LHS, typename RHS>                                                          \
    struct name {                                                                                  \
        struct expr_tag {};                                                                        \
        const LHS& lhs;                                                                            \
        const RHS& rhs;                                                                            \
                                                                                                   \
        auto operator() () const {                                                                 \
            return compute(lhs) op compute(rhs);                                                   \
        }                                                                                          \
                                                                                                   \
        std::string format() const {                                                               \
            return std::format("{} " #op " {}", fmt(lhs), fmt(rhs));                               \
        }                                                                                          \
    };                                                                                             \
                                                                                                   \
    template <typename LHS, typename RHS>                                                          \
    name(const LHS&, const RHS&) -> name<LHS, RHS>;

BINARY_OP(add, +);
BINARY_OP(sub, -);
BINARY_OP(mul, *);
BINARY_OP(eq, ==);
BINARY_OP(ne, !=);
BINARY_OP(lt, <);
BINARY_OP(le, <=);
BINARY_OP(gt, >);
BINARY_OP(ge, >=);

}  // namespace clice
