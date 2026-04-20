/*
 * global_meta_management.cpp
 *
 * Implementation of GlobalMetaManagement (declared in global_meta_management.h).
 */

#include "global_meta_management.h"

std::vector<Status> GlobalMetaManagement::batchInsertGlobal(
    const std::vector<std::string>& keys,
    const std::vector<std::string>& values)
{
    const std::lock_guard<std::mutex> guard(mutex_);
    std::vector<Status> results;
    results.reserve(keys.size());
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (store_.count(keys[i])) {
            results.push_back(Status::error("already exists"));
        } else {
            store_[keys[i]] = values[i];
            results.push_back(Status{});
        }
    }
    return results;
}

std::vector<Result<std::string>> GlobalMetaManagement::batchQueryGlobal(
    const std::vector<std::string>& keys) const
{
    const std::lock_guard<std::mutex> guard(mutex_);
    std::vector<Result<std::string>> results;
    results.reserve(keys.size());
    for (const auto& key : keys) {
        const auto it = store_.find(key);
        if (it == store_.end()) {
            results.push_back({std::string{}, Status::error("not found")});
        } else {
            results.push_back({it->second, Status{}});
        }
    }
    return results;
}

std::vector<Status> GlobalMetaManagement::batchUpdateGlobal(
    const std::vector<std::string>& keys,
    const std::vector<std::string>& values)
{
    const std::lock_guard<std::mutex> guard(mutex_);
    std::vector<Status> results;
    results.reserve(keys.size());
    for (std::size_t i = 0; i < keys.size(); ++i) {
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

std::vector<Status> GlobalMetaManagement::batchDeleteGlobal(
    const std::vector<std::string>& keys)
{
    const std::lock_guard<std::mutex> guard(mutex_);
    std::vector<Status> results;
    results.reserve(keys.size());
    for (const auto& key : keys) {
        const auto it = store_.find(key);
        if (it == store_.end()) {
            results.push_back(Status::error("not found"));
        } else {
            store_.erase(it);
            results.push_back(Status{});
        }
    }
    return results;
}
