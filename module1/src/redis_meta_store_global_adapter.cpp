#include "metastore/redis_meta_store_global_adapter.h"

#include <memory>

#include "metastore/redis_meta_store_backend.h"

namespace metastore {

RedisMetaStoreGlobalAdapter::RedisMetaStoreGlobalAdapter()
    : backend_(std::make_unique<RedisMetaStoreBackend>()) {}

RedisMetaStoreGlobalAdapter::~RedisMetaStoreGlobalAdapter() = default;

Status RedisMetaStoreGlobalAdapter::init(const MetaStoreInitOptions& options) {
    return backend_->init(options);
}

Status RedisMetaStoreGlobalAdapter::reset() {
    return backend_->reset();
}

std::vector<Result<std::string>> RedisMetaStoreGlobalAdapter::batchQueryGlobal(
    const std::vector<std::string>& keys) {
    return backend_->batchQuery(keys);
}

std::vector<Status> RedisMetaStoreGlobalAdapter::batchInsertGlobal(
    const std::vector<std::string>& keys,
    const std::vector<std::string>& values) {
    return backend_->batchInsert(keys, values);
}

std::vector<Status> RedisMetaStoreGlobalAdapter::batchUpdateGlobal(
    const std::vector<std::string>& keys,
    const std::vector<std::string>& values) {
    return backend_->batchUpdate(keys, values);
}

std::vector<Status> RedisMetaStoreGlobalAdapter::batchDeleteGlobal(
    const std::vector<std::string>& keys) {
    return backend_->batchDelete(keys);
}

Status RedisMetaStoreGlobalAdapter::clear() {
    return backend_->clear();
}

}  // namespace metastore
