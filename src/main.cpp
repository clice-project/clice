#include <LSP/Server.h>

int main() {
    using namespace clice;
    Server::Initialize();
    Server::Exit();
    return 0;
}
