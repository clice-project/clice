#define SELF(x) x

int SELF(x) = 1;

int main() {
    SELF(x) = 2;
}
