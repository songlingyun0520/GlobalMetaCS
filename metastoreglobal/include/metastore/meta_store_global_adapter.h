#pragma once

#include <string>
#include <vector>

#include "common/metastore/meta_store_types.h"

namespace metastore {

class MetaStoreGlobalAdapter {
public:
    virtual ~MetaStoreGlobalAdapter() = default;

    virtual Status init(const MetaStoreInitOptions& options) = 0;
    virtual Status reset() = 0;

    virtual std::vector<Result<std::string>> batchQueryGlobal(
        const std::vector<std::string>& keys) = 0;

    virtual std::vector<Status> batchInsertGlobal(
        const std::vector<std::string>& keys,
        const std::vector<std::string>& values) = 0;

    virtual std::vector<Status> batchUpdateGlobal(
        const std::vector<std::string>& keys,
        const std::vector<std::string>& values) = 0;

    virtual std::vector<Status> batchDeleteGlobal(
        const std::vector<std::string>& keys) = 0;

    virtual Status clear() = 0;
};

}  // namespace metastore
