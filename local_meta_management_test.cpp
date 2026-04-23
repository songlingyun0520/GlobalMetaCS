/*
 * local_meta_management_test.cpp
 *
 * Basic test program for LocalMetaManagement.
 * Covers batch CRUD semantics, validation paths, and a small concurrency smoke test.
 *
 * Build:
 *   g++ -std=c++17 -Wall -O2 local_meta_management.cpp \
 *       local_meta_management_test.cpp -o local_meta_management_test -lpthread
 */

#include "local_meta_management.h"

#include <atomic>
#include <iostream>
#include <string>
#include <thread>
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

bool HasMessage(const Status& status, const std::string& expected) {
    return !status.ok() && status.message() == expected;
}

bool HasMessage(const Result<LocalMetaValue>& result, const std::string& expected) {
    return !result.ok() && result.status.message() == expected;
}

void TestInsertQueryAndOrdering(TestContext& t) {
    LocalMetaManagement local;
    const std::vector<std::string> keys = {"k2", "k1"};
    const std::vector<LocalMetaValue> values = {
        {LocalLocationType::kSsdLBA, 200},
        {LocalLocationType::kDramVA, 100},
    };

    const auto insert = local.batchInsertLocal(keys, values);
    t.Expect(insert.size() == 2, "insert returns one status per key");
    t.Expect(insert.size() == 2 && insert[0].ok() && insert[1].ok(),
             "initial insert succeeds");

    const auto query = local.batchQueryLocal(keys);
    t.Expect(query.size() == 2, "query returns one result per key");
    t.Expect(query.size() == 2 && query[0].ok() && query[0].value == values[0],
             "query preserves order for k2");
    t.Expect(query.size() == 2 && query[1].ok() && query[1].value == values[1],
             "query preserves order for k1");
}

void TestDuplicateInsertAndUpdateTransitions(TestContext& t) {
    LocalMetaManagement local;
    const std::string key = "token:1";

    const auto insert = local.batchInsertLocal(
        {key}, {{LocalLocationType::kDramVA, 0x1000}});
    t.Expect(insert.size() == 1 && insert[0].ok(), "first insert succeeds");

    const auto duplicate = local.batchInsertLocal(
        {key}, {{LocalLocationType::kSsdLBA, 0x2000}});
    t.Expect(duplicate.size() == 1, "duplicate insert returns one status");
    t.Expect(duplicate.size() == 1 &&
                 HasMessage(duplicate[0], "already exists"),
             "duplicate insert returns already exists");

    const auto update = local.batchUpdateLocal(
        {key}, {{LocalLocationType::kSsdLBA, 0x3000}});
    t.Expect(update.size() == 1 && update[0].ok(), "VA -> LBA update succeeds");

    const auto query = local.batchQueryLocal({key});
    t.Expect(query.size() == 1 && query[0].ok(), "updated key remains queryable");
    t.Expect(query.size() == 1 &&
                 query[0].value == LocalMetaValue{LocalLocationType::kSsdLBA, 0x3000},
             "query sees latest LBA value");
}

void TestNotFoundAndIdempotentDelete(TestContext& t) {
    LocalMetaManagement local;
    const std::string existing = "existing";
    const std::string missing = "missing";

    local.batchInsertLocal({existing}, {{LocalLocationType::kDramVA, 7}});

    const auto update = local.batchUpdateLocal(
        {existing, missing},
        {{LocalLocationType::kSsdLBA, 8}, {LocalLocationType::kDramVA, 9}});
    t.Expect(update.size() == 2, "mixed update returns one status per key");
    t.Expect(update.size() == 2 && update[0].ok(), "update existing succeeds");
    t.Expect(update.size() == 2 && HasMessage(update[1], "not found"),
             "update missing returns not found");

    const auto del = local.batchDeleteLocal({existing, missing});
    t.Expect(del.size() == 2, "delete returns one status per key");
    t.Expect(del.size() == 2 && del[0].ok(), "delete existing succeeds");
    t.Expect(del.size() == 2 && del[1].ok(), "delete missing is idempotent success");

    const auto query = local.batchQueryLocal({existing});
    t.Expect(query.size() == 1, "post-delete query returns one result");
    t.Expect(query.size() == 1 && HasMessage(query[0], "not found"),
             "deleted key is no longer present");
}

void TestValidationErrors(TestContext& t) {
    LocalMetaManagement local;

    const auto query = local.batchQueryLocal({""});
    t.Expect(query.size() == 1, "empty-key query returns one result");
    t.Expect(query.size() == 1 &&
                 HasMessage(query[0], "invalid argument: key must not be empty"),
             "empty-key query returns invalid argument");

    const auto insert = local.batchInsertLocal(
        {"", "k2"},
        {{LocalLocationType::kDramVA, 1}, {LocalLocationType::kSsdLBA, 2}});
    t.Expect(insert.size() == 2, "mixed insert returns two statuses");
    t.Expect(insert.size() == 2 &&
                 HasMessage(insert[0], "invalid argument: key must not be empty"),
             "empty key insert returns invalid argument");
    t.Expect(insert.size() == 2 && insert[1].ok(),
             "valid key in same insert batch still succeeds");

    const auto mismatch = local.batchInsertLocal(
        {"k1", "k2"},
        {{LocalLocationType::kDramVA, 1}});
    t.Expect(mismatch.size() == 2, "size mismatch returns max(key_count, value_count) statuses");
    t.Expect(mismatch.size() == 2 &&
                 HasMessage(mismatch[0], "invalid argument: keys and values size mismatch"),
             "size mismatch returns invalid argument");
    t.Expect(mismatch.size() == 2 &&
                 HasMessage(mismatch[1], "invalid argument: keys and values size mismatch"),
             "size mismatch marks every returned status");

    const auto invalid_type = local.batchUpdateLocal(
        {"k2"},
        {{static_cast<LocalLocationType>(99), 3}});
    t.Expect(invalid_type.size() == 1, "invalid type update returns one status");
    t.Expect(invalid_type.size() == 1 &&
                 HasMessage(invalid_type[0], "invalid argument: invalid location type"),
             "invalid type is rejected");
}

void TestConcurrentAccessSmoke(TestContext& t) {
    LocalMetaManagement local;
    std::vector<std::string> keys;
    std::vector<LocalMetaValue> values;
    keys.reserve(64);
    values.reserve(64);
    for (std::size_t i = 0; i < 64; ++i) {
        keys.push_back("hot:" + std::to_string(i));
        values.push_back({LocalLocationType::kDramVA, i});
    }

    const auto insert = local.batchInsertLocal(keys, values);
    for (const auto& status : insert) {
        t.Expect(status.ok(), "concurrency setup insert succeeds");
    }

    std::atomic<bool> reader_ok{true};
    std::thread reader([&]() {
        for (int round = 0; round < 200; ++round) {
            const auto result = local.batchQueryLocal({"hot:0", "hot:31", "hot:63"});
            if (result.size() != 3 || !result[0].ok() || !result[1].ok() || !result[2].ok()) {
                reader_ok.store(false);
                return;
            }
        }
    });

    std::thread writer([&]() {
        for (std::size_t i = 0; i < 64; ++i) {
            const auto update = local.batchUpdateLocal(
                {"hot:" + std::to_string(i)},
                {{LocalLocationType::kSsdLBA, 1000 + i}});
            if (update.size() != 1 || !update[0].ok()) {
                reader_ok.store(false);
                return;
            }
        }
    });

    reader.join();
    writer.join();

    t.Expect(reader_ok.load(), "concurrent reader/writer smoke test completes cleanly");

    const auto final = local.batchQueryLocal({"hot:0", "hot:63"});
    t.Expect(final.size() == 2, "final query after concurrency returns two results");
    t.Expect(final.size() == 2 &&
                 final[0].ok() &&
                 final[0].value == LocalMetaValue{LocalLocationType::kSsdLBA, 1000},
             "hot:0 is updated to SSD after concurrency test");
    t.Expect(final.size() == 2 &&
                 final[1].ok() &&
                 final[1].value == LocalMetaValue{LocalLocationType::kSsdLBA, 1063},
             "hot:63 is updated to SSD after concurrency test");
}

}  // namespace

int main() {
    TestContext t;

    TestInsertQueryAndOrdering(t);
    TestDuplicateInsertAndUpdateTransitions(t);
    TestNotFoundAndIdempotentDelete(t);
    TestValidationErrors(t);
    TestConcurrentAccessSmoke(t);

    if (t.failures() != 0) {
        std::cerr << "\nLocalMetaManagement tests failed: " << t.failures() << '\n';
        return 1;
    }

    std::cout << "LocalMetaManagement tests passed\n";
    return 0;
}
