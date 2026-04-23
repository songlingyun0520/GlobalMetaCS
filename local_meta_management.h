#pragma once

/*
 * local_meta_management.h
 *
 * In-process implementation of LocalMetaManagement.
 * Maintains key -> LocalMetaValue mappings for local DRAM / SSD locations.
 * Thread safety: all public methods are protected by a mutex.
 */

#include "gmm_types.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

enum class LocalLocationType {
    kDramVA = 0,
    kSsdLBA = 1,
};

struct LocalMetaValue {
    LocalLocationType type = LocalLocationType::kDramVA;
    std::uint64_t location = 0;
};

inline bool operator==(const LocalMetaValue& lhs, const LocalMetaValue& rhs) {
    return lhs.type == rhs.type && lhs.location == rhs.location;
}

class LocalMetaManagement {
public:
    std::vector<Status> batchInsertLocal(
        const std::vector<std::string>& keys,
        const std::vector<LocalMetaValue>& values);

    std::vector<Result<LocalMetaValue>> batchQueryLocal(
        const std::vector<std::string>& keys) const;

    std::vector<Status> batchUpdateLocal(
        const std::vector<std::string>& keys,
        const std::vector<LocalMetaValue>& values);

    std::vector<Status> batchDeleteLocal(
        const std::vector<std::string>& keys);

private:
    std::unordered_map<std::string, LocalMetaValue> store_;
    mutable std::mutex mutex_;
};
