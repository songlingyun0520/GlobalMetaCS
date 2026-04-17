#pragma once

#include <string>
#include <vector>

#include "metastore/meta_store_types.h"

namespace metastore {

class MetaStoreBackend {
public:
    virtual ~MetaStoreBackend() = default;

    virtual Status init(const MetaStoreInitOptions& options) = 0;
    virtual Status reset() = 0;

    virtual std::vector<Result<std::string>> batchQuery(
        const std::vector<std::string>& keys) = 0;

    virtual std::vector<Status> batchInsert(
        const std::vector<std::string>& keys,
        const std::vector<std::string>& values) = 0;

    virtual std::vector<Status> batchUpdate(
        const std::vector<std::string>& keys,
        const std::vector<std::string>& values) = 0;

    virtual std::vector<Status> batchDelete(
        const std::vector<std::string>& keys) = 0;

    virtual Status clear() = 0;
};

}  // namespace metastore
