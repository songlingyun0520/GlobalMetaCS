#include "test_harness.h"

#include "metastore/meta_store_backend.h"

TEST_CASE(shared_backend_headers_compile_with_module1_public_types) {
    metastore::MetaStoreInitOptions options;
    EXPECT_TRUE(options.backend == metastore::BackendKind::kRedis);
}
