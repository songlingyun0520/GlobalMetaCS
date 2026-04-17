#include "test_harness.h"

using metastore::RedisMetaStoreGlobalAdapter;

namespace {

void InitAndClear(RedisMetaStoreGlobalAdapter* adapter) {
    REQUIRE_OK(adapter->init(TestRedisOptions()));
    REQUIRE_OK(adapter->clear());
}

void ExpectAllInvalidArgument(const std::vector<metastore::Status>& statuses,
                              std::size_t expected_size) {
    EXPECT_EQ(statuses.size(), expected_size);
    for (const auto& status : statuses) {
        EXPECT_TRUE(status.IsInvalidArgument());
    }
}

void ExpectAllMetadataError(const std::vector<metastore::Status>& statuses,
                            std::size_t expected_size) {
    EXPECT_EQ(statuses.size(), expected_size);
    for (const auto& status : statuses) {
        EXPECT_TRUE(status.IsMetadataError());
    }
}

void ExpectAllMetadataError(
    const std::vector<metastore::Result<std::string>>& results,
    std::size_t expected_size) {
    EXPECT_EQ(results.size(), expected_size);
    for (const auto& result : results) {
        EXPECT_TRUE(result.status.IsMetadataError());
    }
}

}  // namespace

// --- 正常路径与按条目语义 ---

TEST_CASE(batch_insert_then_query_preserves_input_order) {
    RedisMetaStoreGlobalAdapter adapter;
    InitAndClear(&adapter);

    EXPECT_ALL_OK(adapter.batchInsertGlobal({"k2", "k1"}, {"v2", "v1"}));
    auto result = adapter.batchQueryGlobal({"k2", "k1"});

    EXPECT_EQ(result.size(), 2u);
    EXPECT_TRUE(result[0].ok());
    EXPECT_EQ(result[0].value, "v2");
    EXPECT_TRUE(result[1].ok());
    EXPECT_EQ(result[1].value, "v1");
}

TEST_CASE(batch_query_supports_mixed_found_and_missing_entries) {
    RedisMetaStoreGlobalAdapter adapter;
    InitAndClear(&adapter);
    EXPECT_ALL_OK(adapter.batchInsertGlobal({"present"}, {"v"}));

    auto result = adapter.batchQueryGlobal({"present", "missing"});
    EXPECT_EQ(result.size(), 2u);
    EXPECT_TRUE(result[0].ok());
    EXPECT_EQ(result[0].value, "v");
    EXPECT_TRUE(result[1].status.IsNotFound());
    EXPECT_EQ(result[1].value, "");
}

TEST_CASE(batch_insert_supports_partial_success_and_preserves_order) {
    RedisMetaStoreGlobalAdapter adapter;
    InitAndClear(&adapter);
    EXPECT_ALL_OK(adapter.batchInsertGlobal({"existing"}, {"v1"}));

    auto result = adapter.batchInsertGlobal({"existing", "new"}, {"v2", "v-new"});
    EXPECT_EQ(result.size(), 2u);
    EXPECT_TRUE(result[0].IsAlreadyExists());
    EXPECT_TRUE(result[1].ok());

    auto query = adapter.batchQueryGlobal({"existing", "new"});
    EXPECT_EQ(query.size(), 2u);
    EXPECT_TRUE(query[0].ok());
    EXPECT_EQ(query[0].value, "v1");
    EXPECT_TRUE(query[1].ok());
    EXPECT_EQ(query[1].value, "v-new");
}

TEST_CASE(batch_update_supports_partial_success_and_preserves_order) {
    RedisMetaStoreGlobalAdapter adapter;
    InitAndClear(&adapter);
    EXPECT_ALL_OK(adapter.batchInsertGlobal({"existing"}, {"v1"}));

    auto result = adapter.batchUpdateGlobal({"existing", "missing"}, {"v2", "nope"});
    EXPECT_EQ(result.size(), 2u);
    EXPECT_TRUE(result[0].ok());
    EXPECT_TRUE(result[1].IsNotFound());

    auto query = adapter.batchQueryGlobal({"existing"});
    EXPECT_EQ(query.size(), 1u);
    EXPECT_TRUE(query[0].ok());
    EXPECT_EQ(query[0].value, "v2");
}

TEST_CASE(batch_delete_handles_existing_and_missing_keys_together) {
    RedisMetaStoreGlobalAdapter adapter;
    InitAndClear(&adapter);
    EXPECT_ALL_OK(adapter.batchInsertGlobal({"existing"}, {"v"}));

    auto result = adapter.batchDeleteGlobal({"existing", "missing"});
    EXPECT_EQ(result.size(), 2u);
    EXPECT_TRUE(result[0].ok());
    EXPECT_TRUE(result[1].ok());

    auto query = adapter.batchQueryGlobal({"existing"});
    EXPECT_EQ(query.size(), 1u);
    EXPECT_TRUE(query[0].status.IsNotFound());
}

// --- 参数校验分支 ---

TEST_CASE(batch_operations_return_validation_error_for_empty_key) {
    RedisMetaStoreGlobalAdapter adapter;
    InitAndClear(&adapter);

    auto query = adapter.batchQueryGlobal({""});
    EXPECT_EQ(query.size(), 1u);
    EXPECT_TRUE(query[0].status.IsInvalidArgument());

    auto insert = adapter.batchInsertGlobal({""}, {"v"});
    ExpectAllInvalidArgument(insert, 1u);

    auto update = adapter.batchUpdateGlobal({""}, {"v"});
    ExpectAllInvalidArgument(update, 1u);

    auto del = adapter.batchDeleteGlobal({""});
    ExpectAllInvalidArgument(del, 1u);
}

TEST_CASE(batch_insert_and_update_return_validation_error_on_size_mismatch) {
    RedisMetaStoreGlobalAdapter adapter;
    InitAndClear(&adapter);

    auto insert = adapter.batchInsertGlobal({"k1", "k2"}, {"v1"});
    ExpectAllInvalidArgument(insert, 2u);

    auto update = adapter.batchUpdateGlobal({"k1", "k2"}, {"v1"});
    ExpectAllInvalidArgument(update, 2u);
}

TEST_CASE(batch_operations_with_empty_key_list_return_empty_results) {
    RedisMetaStoreGlobalAdapter adapter;
    InitAndClear(&adapter);

    auto query = adapter.batchQueryGlobal({});
    EXPECT_EQ(query.size(), 0u);

    auto insert = adapter.batchInsertGlobal({}, {});
    EXPECT_EQ(insert.size(), 0u);

    auto update = adapter.batchUpdateGlobal({}, {});
    EXPECT_EQ(update.size(), 0u);

    auto del = adapter.batchDeleteGlobal({});
    EXPECT_EQ(del.size(), 0u);
}

// --- 适配器状态分支 ---

TEST_CASE(batch_operations_fail_consistently_when_adapter_not_initialized) {
    RedisMetaStoreGlobalAdapter adapter;

    auto query = adapter.batchQueryGlobal({"k1", "k2"});
    ExpectAllMetadataError(query, 2u);

    auto insert = adapter.batchInsertGlobal({"k1", "k2"}, {"v1", "v2"});
    ExpectAllMetadataError(insert, 2u);

    auto update = adapter.batchUpdateGlobal({"k1", "k2"}, {"v1", "v2"});
    ExpectAllMetadataError(update, 2u);

    auto del = adapter.batchDeleteGlobal({"k1", "k2"});
    ExpectAllMetadataError(del, 2u);
}
