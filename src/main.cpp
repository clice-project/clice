#include <Server/Server.h>

int main(int argc, const char** argv) {
    using namespace clice::global;
    return server.run(argc, argv);
}
