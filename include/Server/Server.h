#pragma once

namespace clice {

class Server {
public:
    int run(int argc, const char** argv);
};

namespace global {
extern Server server;
}

}  // namespace clice
