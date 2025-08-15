#include "Test/Test.h"
#include "Server/Server.h"

namespace clice::testing {

namespace {

TEST(ActiveFileManager, Basic) {
    ActiveFileManager manager;

    EXPECT_EQ(manager.max_size(), ActiveFileManager::DefaultMaxActiveFileNum);
    manager.set_capability(0);

    EXPECT_GE(manager.max_size(), 1);
    manager.set_capability(std::numeric_limits<size_t>::max());

    EXPECT_LE(manager.max_size(), ActiveFileManager::UnlimitedActiveFileNum);
}

TEST(ActiveFileManager, LruAlgorithm) {
    ActiveFileManager manager;
    manager.set_capability(1);
    EXPECT_EQ(manager.size(), 0);

    auto& first = manager.add("first", OpenFile{.version = 1});
    EXPECT_EQ(manager.size(), 1);
    EXPECT_TRUE(manager.contains("first"));
    EXPECT_EQ(first->version, 1);

    auto& second = manager.add("second", OpenFile{.version = 2});
    EXPECT_EQ(manager.size(), 1);
    EXPECT_EQ(manager.size(), manager.max_size());
    EXPECT_FALSE(manager.contains("first"));
    EXPECT_TRUE(manager.contains("second"));

    EXPECT_EQ(second->version, 2);
}

TEST(ActiveFileManager, Iterator) {
    ActiveFileManager manager;

    constexpr static size_t TotalInsertedNum = 10;
    constexpr static size_t MaxActiveFileNum = 3;
    manager.set_capability(MaxActiveFileNum);

    // insert file from (1 .. TotalInsertedNum).
    // so there should be (TotalInsertedNum - MaxActiveFileNum) after inserted
    for(uint32_t i = 1; i <= TotalInsertedNum; i++) {
        std::string fpath = std::format("{}", i);
        OpenFile object{.version = i};

        auto& inseted = manager.add(fpath, std::move(object));
        std::optional new_added_entry = manager.get(fpath);
        ASSERT_TRUE(new_added_entry.has_value());
        auto new_added = std::move(new_added_entry).value();
        EXPECT_EQ(inseted, new_added);

        EXPECT_NE(new_added, nullptr);
        EXPECT_EQ(new_added->version, i);

        auto& [path, openfile] = *manager.begin();
        EXPECT_EQ(path, fpath);
        EXPECT_EQ(openfile->version, new_added->version);
    }

    EXPECT_EQ(manager.size(), manager.max_size());

    // the remain file should be in reversed order.
    auto iter = manager.begin();
    int i = TotalInsertedNum;
    while(iter != manager.end()) {
        auto& [path, openfile] = *iter;
        EXPECT_EQ(path, std::to_string(i));
        EXPECT_EQ(openfile->version, i);
        iter++;
        i--;
    }
}

}  // namespace
}  // namespace clice::testing
