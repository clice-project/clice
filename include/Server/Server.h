#pragma once

namespace clice {

class Server {
public:
    int run(int argc, const char** argv);

    void handleMessage(std::string_view message);
};

namespace global {
extern Server server;
}

}  // namespace clice
