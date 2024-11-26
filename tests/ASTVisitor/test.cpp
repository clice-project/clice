void foo [[gnu::format(printf, 1, 3)]] (const char* s, char* buf, ...);

void __attribute__((__format__(printf, 1, 3))) bar(const char* s, char* buf, ...);

void foo2(int x) {
    if(x < 3) [[unlikely]] {}
}
