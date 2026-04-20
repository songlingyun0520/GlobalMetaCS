#pragma once

/*
 * global_meta_management.h
 *
 * In-process implementation of GlobalMetaManagement.
 * Simple key-value store; no distinction between token-key and block-key.
 * Thread safety: all public methods are protected by a mutex.
 */

#include "gmm_types.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class GlobalMetaManagement {
public:
    std::vector<Status>              batchInsertGlobal(const std::vector<std::string>& keys,
                                                       const std::vector<std::string>& values);

    std::vector<Result<std::string>> batchQueryGlobal(const std::vector<std::string>& keys) const;

    std::vector<Status>              batchUpdateGlobal(const std::vector<std::string>& keys,
                                                       const std::vector<std::string>& values);

    std::vector<Status>              batchDeleteGlobal(const std::vector<std::string>& keys);

private:
    std::unordered_map<std::string, std::string> store_;
    mutable std::mutex                           mutex_;
};
