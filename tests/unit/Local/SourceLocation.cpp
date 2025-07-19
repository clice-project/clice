#include "Test/CTest.h"
#include "llvm/Support/CommandLine.h"

namespace clice::testing {

namespace {

llvm::DenseMap<clang::SourceLocation, std::vector<clang::SourceLocation>> deps;

void dumpMacro(clang::SourceManager& SM,
               clang::SourceLocation location,
               std::uint32_t offset,
               std::uint32_t indent = 0) {
    std::string s = std::string(indent * 2, ' ');
    if(location.isMacroID()) {
        auto [fid, offset] = SM.getDecomposedLoc(location);
        auto SLoc = &SM.getSLocEntry(fid);
        assert(SLoc->isExpansion());
        auto expansion = SLoc->getExpansion();
        println("{}fid: {}, offset: {}", s, fid.getHashValue(), offset);
        dumpMacro(SM, expansion.getSpellingLoc(), offset, indent + 1);
        dumpMacro(SM, expansion.getExpansionLocStart(), offset, indent + 1);
    } else {
        s += location.getLocWithOffset(offset).printToString(SM);
        clice::println("{}", s);
    }


}

void dumpLocation(clang::SourceManager& SM, clang::SourceLocation location) {
    /// The location is from source file token.
    if(location.isFileID()) {
        /// True offset and filename
        auto [fid, offset] = SM.getDecomposedLoc(location);
        auto entry = SM.getFileEntryRefForID(fid);
        auto name = entry->getName();

        /// Modified by #line diretive.
        auto loc = SM.getPresumedLoc(location);

        // println("spelling: -------------------------------");
        // println("fid: {}, offset: {}", fid.getHashValue(), offset);
    } else {
        /// The location is from macro expansion.

        SM.isMacroArgExpansion(location);
        SM.isMacroBodyExpansion(location);

        /// SM.getSpellingLoc(location);
        auto location2 = SM.getImmediateSpellingLoc(location);
        /// location2.dump(SM);
        /// auto location3 = SM.getImmediateExpansionRange(location);
        /// auto begin = location3.getBegin();
        /// auto end = location3.getEnd();
        /// begin.dump(SM);
        /// end.dump(SM);

        auto [fid, offset] = SM.getDecomposedLoc(location);
        auto sloc = &SM.getSLocEntry(fid);
        // println("macro: -------------------------------");
        // println("fid: {}, offset: {}", fid.getHashValue(), offset);

        dumpMacro(SM, location, offset);

        SM.getSpellingLoc(location);
    }
}

#define foo(y) x
#define bar(x) foo(x)

TEST(Local, SourceLocation) {
    llvm::StringRef code = R"(
#define foo(x) x
#define bar(x) foo(x)

int bar(x) = 1;
)";
    Tester tester;
    tester.addMain("main.cpp", code);
    tester.compile();
    // auto& SM = tester.unit->srcMgr();
    // auto& TB = tester.unit->tokBuf();
    // for(auto& token: TB.expandedTokens()) {
    //     if(token.text(SM) == "x") {
    //         dumpLocation(SM, token.location());
    //     }
    // }
    //
    // SM.getFileLoc(clang::SourceLocation());
}

}  // namespace

}  // namespace clice::testing
