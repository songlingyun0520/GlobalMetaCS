/*
 * global_meta_client.cpp
 *
 * Implementation of GlobalMetaClient (declared in global_meta_client.h).
 */

#include "global_meta_client.h"

#include <string>

GlobalMetaClient::GlobalMetaClient(CLIENT *clnt) : clnt_(clnt) {}

// ── helpers ───────────────────────────────────────────────────────────────────

Status GlobalMetaClient::rpc_status(gmm_status_result *res,
                                     const char *rpc_name) const {
    if (res == nullptr) {
        return Status::error(std::string(rpc_name) + " RPC call failed: " +
                             clnt_sperror(clnt_, rpc_name));
    }
    return res->status == 0 ? Status{}
                             : Status::error(std::string(rpc_name) + " returned error");
}

// ── single-key operations ─────────────────────────────────────────────────────

Status GlobalMetaClient::rpc_insert(const std::string& key,
                                     const std::string& value) const {
    gmm_kv_arg arg;
    arg.key   = const_cast<char *>(key.c_str());
    arg.value = const_cast<char *>(value.c_str());
    return rpc_status(gmm_insert_1(arg, clnt_), "gmm_insert_1");
}

Result<std::string> GlobalMetaClient::rpc_query(const std::string& key) const {
    gmm_key_arg arg;
    arg.key = const_cast<char *>(key.c_str());
    gmm_query_result *res = gmm_query_1(arg, clnt_);
    if (res == nullptr) {
        return {std::string{}, Status::error("gmm_query_1 RPC call failed: " +
                                              std::string(clnt_sperror(clnt_, "gmm_query_1")))};
    }
    if (!res->found) {
        return {std::string{}, Status::error("not found")};
    }
    return {std::string(res->gmm_query_result_u.value), Status{}};
}

Status GlobalMetaClient::rpc_update(const std::string& key,
                                     const std::string& new_value) const {
    gmm_kv_arg arg;
    arg.key   = const_cast<char *>(key.c_str());
    arg.value = const_cast<char *>(new_value.c_str());
    return rpc_status(gmm_update_1(arg, clnt_), "gmm_update_1");
}

Status GlobalMetaClient::rpc_remove(const std::string& key) const {
    gmm_key_arg arg;
    arg.key = const_cast<char *>(key.c_str());
    return rpc_status(gmm_remove_1(arg, clnt_), "gmm_remove_1");
}

// ── batch operations ──────────────────────────────────────────────────────────

std::vector<Status> GlobalMetaClient::batchInsertGlobal(
    const std::vector<std::string>& keys,
    const std::vector<std::string>& values) const
{
    std::vector<Status> results;
    results.reserve(keys.size());
    for (std::size_t i = 0; i < keys.size(); ++i) {
        results.push_back(rpc_insert(keys[i], values[i]));
    }
    return results;
}

std::vector<Result<std::string>> GlobalMetaClient::batchQueryGlobal(
    const std::vector<std::string>& keys) const
{
    std::vector<Result<std::string>> results;
    results.reserve(keys.size());
    for (const auto& key : keys) {
        results.push_back(rpc_query(key));
    }
    return results;
}

std::vector<Status> GlobalMetaClient::batchUpdateGlobal(
    const std::vector<std::string>& keys,
    const std::vector<std::string>& values) const
{
    std::vector<Status> results;
    results.reserve(keys.size());
    for (std::size_t i = 0; i < keys.size(); ++i) {
        results.push_back(rpc_update(keys[i], values[i]));
    }
    return results;
}

std::vector<Status> GlobalMetaClient::batchDeleteGlobal(
    const std::vector<std::string>& keys) const
{
    std::vector<Status> results;
    results.reserve(keys.size());
    for (const auto& key : keys) {
        results.push_back(rpc_remove(key));
    }
    return results;
}
