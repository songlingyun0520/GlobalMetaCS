#include "test_harness.h"

using metastore::RedisMetaStoreGlobalAdapter;

TEST_CASE(init_prefers_unix_socket_for_local_redis) {
    UnixSocketRedisServer server = UnixSocketRedisServer::Start();

    RedisMetaStoreGlobalAdapter adapter;
    auto options = TestRedisOptions();
    options.endpoint = "127.0.0.1:1";
    options.unix_socket_path = server.socket_path;
    options.prefer_local_socket = true;

    REQUIRE_OK(adapter.init(options));
    REQUIRE_OK(adapter.clear());
    EXPECT_ALL_OK(adapter.batchInsertGlobal({"socket-key"}, {"socket-value"}));

    auto result = adapter.batchQueryGlobal({"socket-key"});
    EXPECT_EQ(result.size(), 1u);
    EXPECT_TRUE(result[0].ok());
    EXPECT_EQ(result[0].value, "socket-value");
}
