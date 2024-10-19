namespace test {

enum TestKind {

};

struct Location {
    int line;
    int column;
};

struct Case {
    consteval Case(Location cursor, TestKind kind, Location result) {}
};

}  // namespace test
