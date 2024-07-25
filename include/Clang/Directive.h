#include <Clang/Clang.h>

namespace clice {

struct MacroDefinition {
    std::string name;
    std::string definition;
    clang::SourceRange range;
};

struct MacroExpansion {
    std::string name;
    std::string expansion;
};

class Directive {
private:
    std::string name;
    clang::SourceRange range;
    std::vector<MacroDefinition> definitions;
    std::vector<MacroExpansion> expansions;

public:
    static std::unique_ptr<Directive> create(clang::SourceManager& sourceManager, clang::SourceRange range);
};

}  // namespace clice
