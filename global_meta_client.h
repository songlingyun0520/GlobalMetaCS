#pragma once

/*
 * global_meta_client.h
 *
 * C++ client wrapper for the GlobalMetaManagement RPC interface.
 * Follows the same Status / Result<T> / batch* pattern as demo.cpp.
 * No distinction between token-key and block-key; all operations use
 * plain string keys and values.
 */

extern "C" {
#include "global_meta.h"
}

#include "gmm_types.h"

#include <string>
#include <vector>

class GlobalMetaClient {
public:
    explicit GlobalMetaClient(CLIENT *clnt);

    std::vector<Status>              batchInsertGlobal(const std::vector<std::string>& keys,
                                                       const std::vector<std::string>& values) const;

    std::vector<Result<std::string>> batchQueryGlobal(const std::vector<std::string>& keys) const;

    std::vector<Status>              batchUpdateGlobal(const std::vector<std::string>& keys,
                                                       const std::vector<std::string>& values) const;

    std::vector<Status>              batchDeleteGlobal(const std::vector<std::string>& keys) const;

private:
    Status         rpc_insert(const std::string& key, const std::string& value) const;
    Result<std::string> rpc_query(const std::string& key) const;
    Status         rpc_update(const std::string& key, const std::string& new_value) const;
    Status         rpc_remove(const std::string& key) const;

    Status rpc_status(gmm_status_result *res, const char *rpc_name) const;

    CLIENT *clnt_;
};
