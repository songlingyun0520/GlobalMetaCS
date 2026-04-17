#include "test_harness.h"

using metastore::BackendKind;
using metastore::MetaStoreInitOptions;
using metastore::RedisMetaStoreGlobalAdapter;

TEST_CASE(init_succeeds_against_local_redis) {
    RedisMetaStoreGlobalAdapter adapter;
    auto options = TestRedisOptions();
    EXPECT_TRUE(adapter.init(options).ok());
}

TEST_CASE(init_rejects_empty_endpoint_for_redis) {
    RedisMetaStoreGlobalAdapter adapter;
    MetaStoreInitOptions options;
    options.backend = BackendKind::kRedis;
    auto status = adapter.init(options);
    EXPECT_TRUE(status.IsInvalidArgument());
}

TEST_CASE(reset_is_idempotent) {
    RedisMetaStoreGlobalAdapter adapter;
    EXPECT_TRUE(adapter.reset().ok());
    EXPECT_TRUE(adapter.reset().ok());
}
