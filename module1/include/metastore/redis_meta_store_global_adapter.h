#pragma once

#include <memory>
#include <vector>

#include "metastore/meta_store_backend.h"
#include "metastore/meta_store_global_adapter.h"

namespace metastore {

class RedisMetaStoreGlobalAdapter : public MetaStoreGlobalAdapter {
public:
    RedisMetaStoreGlobalAdapter();
    ~RedisMetaStoreGlobalAdapter() override;

    Status init(const MetaStoreInitOptions& options) override;
    Status reset() override;

    std::vector<Result<std::string>> batchQueryGlobal(
        const std::vector<std::string>& keys) override;

    std::vector<Status> batchInsertGlobal(
        const std::vector<std::string>& keys,
        const std::vector<std::string>& values) override;

    std::vector<Status> batchUpdateGlobal(
        const std::vector<std::string>& keys,
        const std::vector<std::string>& values) override;

    std::vector<Status> batchDeleteGlobal(
        const std::vector<std::string>& keys) override;

    Status clear() override;

private:
    std::unique_ptr<MetaStoreBackend> backend_;
};

}  // namespace metastore
