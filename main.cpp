
template <typename T>
struct X {
    static void fooooooooooooooo(int x);

    void bar() {
        X<int> x;
        // X<int>::bar
    }

    static void bar2();
};

