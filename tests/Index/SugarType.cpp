namespace test {

struct X {};

struct X x;  // elaborated type

}  // namespace test

using test::X;

X x2;  // using type

using X2 = test::X;

X2 x3;  // typedef type
