#include <print>

#include "Test/Test.h"
#include "Support/StructedText.h"

namespace clice::testing {
TEST(StructedText, Paragraph) {
    constexpr const char* cb =
        R"c(// Without processing, recommended
char *longestPalindrome_solv2(const char *s) {
    int len = strlen(s);
    int max_start = 0;
    int max_len = 0;
    for (int i = 0; i < len; ++i) {
        // j = 0, max_len is odd
        // j = 1, max_len is even
        for (int j = 0; j <= 1; ++j) {
            int l = i;
            int r = i + j;

            // expand the range from center
            while (l >= 0 && r < len && s[l] == s[r]) {
                --l;
                ++r;
            }

            ++l;
            --r;

            if (max_len < r - l + 1) {
                max_len = r - l + 1;
                max_start = i;
            }
        }
    }
    char *res = (char *)malloc((max_len + 1) * sizeof(char));
    memcpy(res, s + max_start, max_len);
    res[max_len] = '\0';
    return res;
}
)c";
    StructedText st;
    st.addParagraph().appendText("CodeBlock Example:").appendNewlineChar();
    st.addCodeBlock(cb, "c");
    auto& para = st.addParagraph();
    para.appendText("para1").appendNewlineChar();
    std::print("{}", st.asMarkdown());
}

TEST(StructedText, BulletList) {
    StructedText st;
    st.addBulletList().addItem().addParagraph().appendText("Item1");
    st.addBulletList().addItem().addParagraph().appendText("Item2", Paragraph::Kind::InlineCode);
    st.addBulletList().addItem().addParagraph().appendText("Item3", Paragraph::Kind::Bold);
    st.addBulletList().addItem().addParagraph().appendText("Item4", Paragraph::Kind::Italic);
    st.addBulletList().addItem().addParagraph().appendText("Item5", Paragraph::Kind::Strikethough);
    std::print("{}", st.asMarkdown());
}

TEST(StructedText, FullText) {
    StructedText st;
    st.addHeading(3)
        .appendText("function")
        .appendText("test_bar", Paragraph::Kind::InlineCode)
        .appendNewlineChar()
        .appendText("Provided by:")
        .appendText("`foo/bar/baz.h`");
    st.addRuler();
    st.addParagraph()
        .appendText("→")
        .appendText("int", Paragraph::Kind::InlineCode)
        .appendNewlineChar();
    st.addParagraph().appendText("Paramaters:", Paragraph::Kind::Bold).appendNewlineChar();
    auto& params = st.addBulletList();
    params.addItem()
        .addParagraph()
        .appendText("int foo", Paragraph::Kind::InlineCode)
        .appendText("doc for foo\ndoc for foo line2");
    params.addItem()
        .addParagraph()
        .appendText("char** bar", Paragraph::Kind::InlineCode)
        .appendText("doc for bar");
    params.addItem()
        .addParagraph()
        .appendText("char** baz", Paragraph::Kind::InlineCode)
        .appendText("doc for baz");
    st.addParagraph().appendText(R"md(
brief block
brief line2
  a b c d e f
  ~~~~^
This is *Italic* **Bold** ~~Striketough~~, `InlineCode` 
)md");
    st.addRuler();
    st.addParagraph().appendText("Details:", Paragraph::Kind::Bold).appendNewlineChar();
    auto& details = st.addBulletList();
    details.addItem().addParagraph().appendText("Detail1: blah blah...");
    details.addItem().addParagraph().appendText("Detail2: blah blah...\n    Line2: ......");
    details.addItem().addParagraph().appendText("Detail3: blah blah...");
    st.addRuler();
    st.addParagraph().appendText("Details:", Paragraph::Kind::Bold).appendNewlineChar();
    auto& warnings = st.addBulletList();
    warnings.addItem().addParagraph().appendText("warnings1: blah blah...");
    warnings.addItem().addParagraph().appendText("warnings2: blah blah...\n    Line2: ......");
    warnings.addItem().addParagraph().appendText("warnings3: blah blah...");
    st.addRuler();
    st.addCodeBlock("int test_bar(int foo, char **bar, char **baz);\n", "cpp");
    std::print("{}", st.asMarkdown());
}

}  // namespace clice::testing
