#include "Test/Test.h"
#include "Server/Server.h"

namespace clice::testing {

namespace {

suite<"ActiveFileManager"> active_file_manager = [] {
    using Manager = ActiveFileManager;

    test("MaxSize") = [] {
        Manager actives;

        expect(that % actives.max_size() == Manager::DefaultMaxActiveFileNum);

        actives.set_capability(0);
        expect(that % actives.max_size() == 1);

        actives.set_capability(std::numeric_limits<size_t>::max());
        expect(that % actives.max_size() <= Manager::UnlimitedActiveFileNum);
    };

    test("LruAlgorithm") = [] {
        Manager actives;
        actives.set_capability(1);

        expect(that % actives.size() == 0);

        auto& first = actives.add("first", OpenFile{.version = 1});
        expect(that % actives.size() == 1);
        expect(that % actives.contains("first") == true);
        expect(that % first->version == 1);

        auto& second = actives.add("second", OpenFile{.version = 2});
        expect(that % actives.size() == 1);
    };

    test("IteratorBasic") = [] {
        Manager actives;
        actives.set_capability(3);

        actives.add("first", OpenFile{.version = 1});
        actives.add("second", OpenFile{.version = 2});
        actives.add("third", OpenFile{.version = 3});
        expect(that % actives.size() == 3);

        auto iter = actives.begin();
        expect(that % iter != actives.end());
        expect(that % iter->first == "third");
        expect(that % iter->second->version == 3);

        iter++;
        expect(that % iter != actives.end());
        expect(that % iter->first == "second");
        expect(that % iter->second->version == 2);

        iter++;
        expect(that % iter != actives.end());
        expect(that % iter->first == "first");
        expect(that % iter->second->version == 1);

        iter++;
        expect(iter == actives.end());
    };

    test("IteratorCheck") = [] {
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
            std::optional new_added_entry = manager.get_or_add(fpath);
            expect(that % new_added_entry.has_value());
            auto new_added = std::move(new_added_entry).value();
            expect(that % inseted == new_added);
            expect(that % new_added != nullptr);
            expect(that % new_added->version == i);

            auto& [path, openfile] = *manager.begin();
            expect(that % path == fpath);
            expect(that % openfile->version == new_added->version);
        }

        expect(that % manager.size() == manager.max_size());

        // the remain file should be in reversed order.
        auto iter = manager.begin();
        int i = TotalInsertedNum;
        while(iter != manager.end()) {
            auto& [path, openfile] = *iter;
            expect(that % path == std::to_string(i));
            expect(that % openfile->version == i);
            iter++;
            i--;
        }
    };
};

}  // namespace
}  // namespace clice::testing
