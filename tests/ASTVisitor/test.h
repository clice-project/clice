template <typename T>
struct X;

using Z = X<int>;

template <>
struct X<int> {
    using type = int;
};

int main() {
    Z::type x = 1;
}
