#include "Server/Server.h"

using namespace clice;

int main(int argc, const char** argv) {
    Server server;
    return server.run(argc, argv);
}

