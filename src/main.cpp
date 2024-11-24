#include "Server/Server.h"

using namespace clice;

int main(int argc, const char** argv) {
    Server server;
    server.run(argc, argv);
    return 0;
}

