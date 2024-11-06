#define Name name
#define SELF(x) x
#define SELF3(x) X

void Name() {}

int SELF(x) = 1;

void SELF3(x)() {}

int z = SELF(x) + SELF(x);