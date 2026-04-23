#include "local_meta_management.h"

#include <algorithm>

namespace {

Status InvalidArgument(const std::string& message) {
    return Status::error("invalid argument: " + message);
}

bool IsValidLocationType(LocalLocationType type) {
    switch (type) {
    case LocalLocationType::kDramVA:
    case LocalLocationType::kSsdLBA:
        return true;
    default:
        return false;
    }
}

Status ValidateKey(const std::string& key) {
    if (key.empty()) {
        return InvalidArgument("key must not be empty");
    }
    return Status{};
}

Status ValidateValue(const LocalMetaValue& value) {
    if (!IsValidLocationType(value.type)) {
        return InvalidArgument("invalid location type");
    }
    return Status{};
}

std::vector<Status> MakeBatchErrorStatuses(std::size_t count, const Status& status) {
    return std::vector<Status>(count, status);
}

std::size_t SizeMismatchResultCount(std::size_t key_count, std::size_t value_count) {
    return std::max(key_count, value_count);
}

}  // namespace

std::vector<Status> LocalMetaManagement::batchInsertLocal(
    const std::vector<std::string>& keys,
    const std::vector<LocalMetaValue>& values)
{
    if (keys.empty() && values.empty()) {
        return {};
    }
    if (keys.size() != values.size()) {
        return MakeBatchErrorStatuses(
            SizeMismatchResultCount(keys.size(), values.size()),
            InvalidArgument("keys and values size mismatch"));
    }

    const std::lock_guard<std::mutex> guard(mutex_);
    std::vector<Status> results;
    results.reserve(keys.size());
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const Status key_status = ValidateKey(keys[i]);
        if (!key_status.ok()) {
            results.push_back(key_status);
            continue;
        }

        const Status value_status = ValidateValue(values[i]);
        if (!value_status.ok()) {
            results.push_back(value_status);
            continue;
        }

        if (store_.count(keys[i]) != 0U) {
            results.push_back(Status::error("already exists"));
        } else {
            store_[keys[i]] = values[i];
            results.push_back(Status{});
        }
    }
    return results;
}

std::vector<Result<LocalMetaValue>> LocalMetaManagement::batchQueryLocal(
    const std::vector<std::string>& keys) const
{
    if (keys.empty()) {
        return {};
    }

    const std::lock_guard<std::mutex> guard(mutex_);
    std::vector<Result<LocalMetaValue>> results;
    results.reserve(keys.size());
    for (const auto& key : keys) {
        const Status key_status = ValidateKey(key);
        if (!key_status.ok()) {
            results.push_back({LocalMetaValue{}, key_status});
            continue;
        }

        const auto it = store_.find(key);
        if (it == store_.end()) {
            results.push_back({LocalMetaValue{}, Status::error("not found")});
        } else {
            results.push_back({it->second, Status{}});
        }
    }
    return results;
}

std::vector<Status> LocalMetaManagement::batchUpdateLocal(
    const std::vector<std::string>& keys,
    const std::vector<LocalMetaValue>& values)
{
    if (keys.empty() && values.empty()) {
        return {};
    }
    if (keys.size() != values.size()) {
        return MakeBatchErrorStatuses(
            SizeMismatchResultCount(keys.size(), values.size()),
            InvalidArgument("keys and values size mismatch"));
    }

    const std::lock_guard<std::mutex> guard(mutex_);
    std::vector<Status> results;
    results.reserve(keys.size());
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const Status key_status = ValidateKey(keys[i]);
        if (!key_status.ok()) {
            results.push_back(key_status);
            continue;
        }

        const Status value_status = ValidateValue(values[i]);
        if (!value_status.ok()) {
            results.push_back(value_status);
            continue;
        }

        const auto it = store_.find(keys[i]);
        if (it == store_.end()) {
            results.push_back(Status::error("not found"));
        } else {
            it->second = values[i];
            results.push_back(Status{});
        }
    }
    return results;
}

std::vector<Status> LocalMetaManagement::batchDeleteLocal(
    const std::vector<std::string>& keys)
{
    if (keys.empty()) {
        return {};
    }

    const std::lock_guard<std::mutex> guard(mutex_);
    std::vector<Status> results;
    results.reserve(keys.size());
    for (const auto& key : keys) {
        const Status key_status = ValidateKey(key);
        if (!key_status.ok()) {
            results.push_back(key_status);
            continue;
        }

        const auto it = store_.find(key);
        if (it != store_.end()) {
            store_.erase(it);
        }
        results.push_back(Status{});
    }
    return results;
}
