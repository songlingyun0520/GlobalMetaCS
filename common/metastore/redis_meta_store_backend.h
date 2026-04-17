#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "metastore/meta_store_backend.h"

struct redisContext;

namespace metastore {

class RedisMetaStoreBackend : public MetaStoreBackend {
public:
    RedisMetaStoreBackend();
    ~RedisMetaStoreBackend() override;

    Status init(const MetaStoreInitOptions& options) override;
    Status reset() override;

    std::vector<Result<std::string>> batchQuery(
        const std::vector<std::string>& keys) override;

    std::vector<Status> batchInsert(
        const std::vector<std::string>& keys,
        const std::vector<std::string>& values) override;

    std::vector<Status> batchUpdate(
        const std::vector<std::string>& keys,
        const std::vector<std::string>& values) override;

    std::vector<Status> batchDelete(
        const std::vector<std::string>& keys) override;

    Status clear() override;

private:
    Status validateConnected() const;
    Status validateKeys(const std::vector<std::string>& keys) const;
    Status validateKeyValues(const std::vector<std::string>& keys,
                             const std::vector<std::string>& values) const;
    std::vector<Status> makeErrorStatuses(std::size_t count,
                                          const Status& status) const;
    std::vector<Result<std::string>> makeErrorResults(
        std::size_t count, const Status& status) const;

    MetaStoreInitOptions options_;
    bool initialized_ = false;
    redisContext* redis_ctx_ = nullptr;
};

}  // namespace metastore
