
#include <iostream>
#include <string>
#include <vector>

#include "metastore/meta_store_backend.h"
#include "metastore/redis_meta_store_global_adapter.h"

namespace {

void PrintStatus(const std::string& step, const metastore::Status& status) {
    std::cout << step << ": " << (status.ok() ? "OK" : "FAILED");
    if (!status.ok()) {
        std::cout << " (" << status.message() << ")";
    }
    std::cout << '\n';
}

void PrintBatchStatuses(const std::string& step,
                        const std::vector<metastore::Status>& statuses) {
    std::cout << step << ":\n";
    for (std::size_t i = 0; i < statuses.size(); ++i) {
        const auto& status = statuses[i];
        std::cout << "  [" << i << "] " << (status.ok() ? "OK" : "FAILED");
        if (!status.ok()) {
            std::cout << " (" << status.message() << ")";
        }
        std::cout << '\n';
    }
}

void PrintQueryResults(const std::vector<std::string>& keys,
                       const std::vector<metastore::Result<std::string>>& results) {
    std::cout << "query results:\n";
    for (std::size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        std::cout << "  key=" << keys[i] << " -> ";
        if (result.ok()) {
            std::cout << result.value;
        } else {
            std::cout << "FAILED (" << result.status.message() << ")";
        }
        std::cout << '\n';
    }
}

}  // namespace

int main() {
    metastore::RedisMetaStoreGlobalAdapter adapter;

    metastore::MetaStoreInitOptions options;
    options.backend = metastore::BackendKind::kRedis;
    options.endpoint = "127.0.0.1:6379";
    options.db_index = 0;
    options.enable_clear = true;

    const auto init_status = adapter.init(options);
    PrintStatus("init", init_status);
    if (!init_status.ok()) {
        std::cerr << "Please make sure Redis is running at " << options.endpoint << '\n';
        return 1;
    }

    PrintStatus("clear", adapter.clear());

    const std::vector<std::string> keys = {"user:1", "user:2"};
    const std::vector<std::string> values = {"Alice", "Bob"};

    auto insert_statuses = adapter.batchInsertGlobal(keys, values);
    PrintBatchStatuses("batchInsertGlobal", insert_statuses);

    auto query_results = adapter.batchQueryGlobal(keys);
    PrintQueryResults(keys, query_results);

    auto update_statuses = adapter.batchUpdateGlobal({"user:2"}, {"Bobby"});
    PrintBatchStatuses("batchUpdateGlobal", update_statuses);

    auto query_after_update = adapter.batchQueryGlobal({"user:2"});
    PrintQueryResults({"user:2"}, query_after_update);

    auto delete_statuses = adapter.batchDeleteGlobal({"user:1", "user:2"});
    PrintBatchStatuses("batchDeleteGlobal", delete_statuses);

    return 0;
}