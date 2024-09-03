#include <AST/ParsedAST.h>
#include <Server/Option.h>
#include <Server/Command.h>

int main(int argc, const char** argv) {
    using namespace clice;
    global::option.parse(argc, argv);
    global::CDB.load(global::option.compile_commands_directory);
}
