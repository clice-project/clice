namespace {
template <typename T>
struct A {};

template <>
struct A<int> {};

}  // namespace

int main() {
    A<int> a;
    return 0;
}
