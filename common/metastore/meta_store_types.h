#pragma once

#include <string>

#include "metastoreglobal/include/metastore/status.h"

namespace metastore {

enum class BackendKind {
    kRedis,
    kFile,
    kMemory,
};

struct MetaStoreInitOptions {
    BackendKind backend = BackendKind::kRedis;
    std::string endpoint;
    int db_index = 0;
    std::string password;
    std::string unix_socket_path;
    bool prefer_local_socket = true;
    std::string file_path;
    bool create_if_missing = true;
    bool enable_clear = false;
    bool db_exclusive = false;
};

template <typename T>
struct Result {
    Status status;
    T value;

    bool ok() const { return status.ok(); }
};

}  // namespace metastore
