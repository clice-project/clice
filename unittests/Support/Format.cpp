#include "Test/Test.h"
#include "Support/Format.h"

namespace clice::testing {

namespace {

template <typename Object>
std::string dump(Object&& object) {
    using T = std::remove_cvref_t<Object>;
    if constexpr(refl::special_enum<T>) {
        return std::format("{}", object.name());
    } else if constexpr(std::ranges::input_range<T>) {
        if constexpr(std::same_as<T, std::remove_cvref_t<std::ranges::range_reference_t<T>>>) {
            static_assert(dependent_false<T>, "Cannot dump range");
        } else if constexpr(requires { typename T::key_type; }) {
            if constexpr(requires { typename T::mapped_type; }) {
                /// map

            } else {
                /// set
            }
        } else {
            /// sequence
        }
    } else if constexpr(refl::reflectable_struct<T>) {

    } else {
        static_assert(dependent_false<T>, "Cannot dump object");
    }
}

TEST(Support, Dump) {



}

}  // namespace

}  // namespace clice::testing
