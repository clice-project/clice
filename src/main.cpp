#include <Server/Server.h>

int main(int argc, const char** argv) {
    using namespace clice;
    return global::server.run(argc, argv);
}
