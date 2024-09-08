#pragma once

#include <Server/Option.h>
#include <Server/Command.h>
#include <Server/Scheduler .h>

namespace clice {

class Server {
public:
    int run(int argc, const char** argv);

    void handleMessage(std::string_view message);

private:
    Option option;
    Scheduler scheduler;
    CompilationDatabase CDB;
};

namespace global {
extern Server server;
}

}  // namespace clice
