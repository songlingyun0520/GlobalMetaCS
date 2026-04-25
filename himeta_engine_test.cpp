/*
 * himeta_engine_test.cpp
 *
 * Basic test program for HiMetaEngine.
 * Covers direct exposure of HiIndex / GlobalMetaClient / LocalMetaManagement.
 */

#include "himeta_engine.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

class TestContext {
public:
    void Expect(bool condition, const std::string& message) {
        if (!condition) {
            ++failures_;
            std::cerr << "[FAIL] " << message << '\n';
        }
    }

    int failures() const { return failures_; }

private:
    int failures_ = 0;
};

class FakeHiIndex : public HiIndex {
public:
    std::unordered_map<IndexValue, IndexQueryResult> query_map;

    std::vector<IndexQueryResult> BatchQueryIndex(
        RequestID,
        LayerID,
        const std::vector<IndexValue>& index_list) const override {
        std::vector<IndexQueryResult> results;
        results.reserve(index_list.size());
        for (const auto& index : index_list) {
            const auto it = query_map.find(index);
            if (it == query_map.end()) {
                results.push_back({IndexQueryKind::kNull, ""});
            } else {
                results.push_back(it->second);
            }
        }
        return results;
    }

    std::vector<Status> BatchInsertIndex(
        RequestID,
        LayerID,
        const std::vector<IndexValue>& index_list,
        const std::vector<Address>& address_list) override {
        if (index_list.size() != address_list.size()) {
            return std::vector<Status>(index_list.size(),
                                       Status::error("size mismatch"));
        }
        for (std::size_t i = 0; i < index_list.size(); ++i) {
            query_map[index_list[i]] = {IndexQueryKind::kAddress, address_list[i]};
        }
        return std::vector<Status>(index_list.size(), Status{});
    }

    std::vector<Status> BatchUpdateIndex(
        RequestID,
        LayerID,
        const std::vector<IndexValue>& index_list,
        const std::vector<Address>& address_list) override {
        return BatchInsertIndex(0, 0, index_list, address_list);
    }

    std::vector<Status> BatchDeleteIndex(
        RequestID,
        LayerID,
        const std::vector<IndexValue>& index_list) override {
        for (const auto& index : index_list) {
            query_map.erase(index);
        }
        return std::vector<Status>(index_list.size(), Status{});
    }
};

void TestAccessors(TestContext& t) {
    FakeHiIndex hi_index;
    LocalMetaManagement local_meta;
    HiMetaEngine engine(hi_index, local_meta);

    t.Expect(&engine.hiIndex() == &hi_index, "hiIndex accessor returns injected object");
    t.Expect(&engine.localMetaManagement() == &local_meta,
             "localMetaManagement accessor returns injected object");
    t.Expect(!engine.hasGlobalMetaClient(),
             "globalMetaClient is optional and absent by default");
}

void TestHiIndexOperationsStillOwnedByInjectedObject(TestContext& t) {
    FakeHiIndex hi_index;
    LocalMetaManagement local_meta;
    HiMetaEngine engine(hi_index, local_meta);

    const auto insert = engine.hiIndex().BatchInsertIndex(
        1, 0, {"idx:1", "idx:2"}, {"ADDR:1", "ADDR:2"});
    t.Expect(insert.size() == 2 && insert[0].ok() && insert[1].ok(),
             "hiIndex insert is forwarded through exposed capability");

    const auto query = engine.hiIndex().BatchQueryIndex(2, 0, {"idx:1", "idx:2", "idx:3"});
    t.Expect(query.size() == 3, "hiIndex query returns one result per input index");
    t.Expect(query.size() == 3 &&
                 query[0].kind == IndexQueryKind::kAddress &&
                 query[0].value == "ADDR:1",
             "inserted idx:1 can be queried through exposed hiIndex");
    t.Expect(query.size() == 3 &&
                 query[1].kind == IndexQueryKind::kAddress &&
                 query[1].value == "ADDR:2",
             "inserted idx:2 can be queried through exposed hiIndex");
    t.Expect(query.size() == 3 &&
                 query[2].kind == IndexQueryKind::kNull,
             "missing index stays null");

    const auto del = engine.hiIndex().BatchDeleteIndex(3, 0, {"idx:1"});
    t.Expect(del.size() == 1 && del[0].ok(),
             "hiIndex delete is forwarded through exposed capability");

    const auto after_delete = engine.hiIndex().BatchQueryIndex(4, 0, {"idx:1"});
    t.Expect(after_delete.size() == 1 &&
                 after_delete[0].kind == IndexQueryKind::kNull,
             "deleted index is no longer present");
}

void TestLocalMetaOperationsStillOwnedByInjectedObject(TestContext& t) {
    FakeHiIndex hi_index;
    LocalMetaManagement local_meta;
    HiMetaEngine engine(hi_index, local_meta);

    const auto insert = engine.localMetaManagement().batchInsertLocal(
        {"token:1", "block:1"},
        {
            {LocalLocationType::kDramVA, 100},
            {LocalLocationType::kSsdLBA, 200},
        });
    t.Expect(insert.size() == 2 && insert[0].ok() && insert[1].ok(),
             "localMeta insert is forwarded through exposed capability");

    const auto query = engine.localMetaManagement().batchQueryLocal(
        {"token:1", "block:1", "missing"});
    t.Expect(query.size() == 3, "localMeta query returns one result per key");
    t.Expect(query.size() == 3 &&
                 query[0].ok() &&
                 query[0].value == LocalMetaValue{LocalLocationType::kDramVA, 100},
             "token key is queryable through exposed localMetaManagement");
    t.Expect(query.size() == 3 &&
                 query[1].ok() &&
                 query[1].value == LocalMetaValue{LocalLocationType::kSsdLBA, 200},
             "block key is queryable through exposed localMetaManagement");
    t.Expect(query.size() == 3 &&
                 !query[2].ok() &&
                 query[2].status.message() == "not found",
             "missing local meta key returns not found");
}

void TestGlobalMetaAccessorThrowsWhenUnset(TestContext& t) {
    FakeHiIndex hi_index;
    LocalMetaManagement local_meta;
    HiMetaEngine engine(hi_index, local_meta);

    bool threw = false;
    try {
        (void)engine.globalMetaClient();
    } catch (const std::logic_error&) {
        threw = true;
    }
    t.Expect(threw, "globalMetaClient accessor throws when client is not configured");
}

}  // namespace

int main() {
    TestContext t;

    TestAccessors(t);
    TestHiIndexOperationsStillOwnedByInjectedObject(t);
    TestLocalMetaOperationsStillOwnedByInjectedObject(t);
    TestGlobalMetaAccessorThrowsWhenUnset(t);

    if (t.failures() != 0) {
        std::cerr << "\nHiMetaEngine tests failed: " << t.failures() << '\n';
        return 1;
    }

    std::cout << "HiMetaEngine tests passed\n";
    return 0;
}
