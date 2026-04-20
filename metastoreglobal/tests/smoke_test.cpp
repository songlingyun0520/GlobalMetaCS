#include "test_harness.h"

TEST_CASE(adapter_builds_without_custom_protocol_client) {
    metastore::RedisMetaStoreGlobalAdapter adapter;
    EXPECT_TRUE(true);
}
