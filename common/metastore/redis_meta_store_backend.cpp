#include "metastore/redis_meta_store_backend.h"

#include <hiredis/hiredis.h>

#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace metastore {

namespace {

class RedisReplyGuard {
public:
    explicit RedisReplyGuard(redisReply* reply) : reply_(reply) {}
    ~RedisReplyGuard() {
        if (reply_ != nullptr) {
            freeReplyObject(reply_);
        }
    }

    redisReply* get() const { return reply_; }
    redisReply* operator->() const { return reply_; }

private:
    redisReply* reply_;
};

bool IsLocalRedisEndpoint(const std::string& endpoint) {
    std::size_t separator = endpoint.rfind(':');
    std::string host =
        separator == std::string::npos ? endpoint : endpoint.substr(0, separator);
    return host == "127.0.0.1" || host == "localhost" || host == "::1";
}

Status ParseEndpoint(const std::string& endpoint, std::string* host, int* port) {
    std::size_t separator = endpoint.rfind(':');
    if (separator == std::string::npos || separator == 0 ||
        separator + 1 >= endpoint.size()) {
        return Status::InvalidArgument("endpoint must be host:port");
    }

    *host = endpoint.substr(0, separator);
    try {
        *port = std::stoi(endpoint.substr(separator + 1));
    } catch (...) {
        return Status::InvalidArgument("port must be numeric");
    }
    return Status::OK();
}

Status RedisContextError(redisContext* ctx, const std::string& action) {
    if (ctx == nullptr) {
        return Status::MetadataError(action + " failed: null redis context");
    }
    if (ctx->err != 0) {
        return Status::MetadataError(action + " failed: " +
                                     std::string(ctx->errstr));
    }
    return Status::OK();
}

Status ConnectViaTcp(const std::string& endpoint, redisContext** ctx) {
    std::string host;
    int port = 0;
    Status parse_status = ParseEndpoint(endpoint, &host, &port);
    if (!parse_status.ok()) {
        return parse_status;
    }

    *ctx = redisConnect(host.c_str(), port);
    return RedisContextError(*ctx, "tcp connect");
}

Status ConnectViaUnixSocket(const std::string& socket_path, redisContext** ctx) {
    if (socket_path.empty()) {
        return Status::InvalidArgument("unix socket path is empty");
    }

    *ctx = redisConnectUnix(socket_path.c_str());
    return RedisContextError(*ctx, "unix socket connect");
}

Status ExecCommandArgv(redisContext* ctx, const std::vector<std::string>& parts,
                       redisReply** reply) {
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    argv.reserve(parts.size());
    argvlen.reserve(parts.size());
    for (const std::string& part : parts) {
        argv.push_back(part.data());
        argvlen.push_back(part.size());
    }

    *reply = static_cast<redisReply*>(
        redisCommandArgv(ctx, static_cast<int>(parts.size()), argv.data(),
                         argvlen.data()));
    if (*reply == nullptr) {
        return RedisContextError(ctx, "redis command");
    }
    return Status::OK();
}

Status AppendCommandArgv(redisContext* ctx,
                         const std::vector<std::string>& parts) {
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    argv.reserve(parts.size());
    argvlen.reserve(parts.size());
    for (const std::string& part : parts) {
        argv.push_back(part.data());
        argvlen.push_back(part.size());
    }

    int rc = redisAppendCommandArgv(ctx, static_cast<int>(parts.size()),
                                    argv.data(), argvlen.data());
    if (rc != REDIS_OK) {
        return RedisContextError(ctx, "redis append command");
    }
    return Status::OK();
}

Status FetchPipelineReply(redisContext* ctx, redisReply** reply) {
    void* raw_reply = nullptr;
    int rc = redisGetReply(ctx, &raw_reply);
    if (rc != REDIS_OK) {
        return RedisContextError(ctx, "redis pipeline reply");
    }
    *reply = static_cast<redisReply*>(raw_reply);
    if (*reply == nullptr) {
        return Status::MetadataError("redis pipeline reply failed: null reply");
    }
    return Status::OK();
}

Status AuthenticateIfNeeded(redisContext* ctx, const std::string& password) {
    if (password.empty()) {
        return Status::OK();
    }

    redisReply* raw_reply = nullptr;
    Status exec_status = ExecCommandArgv(ctx, {"AUTH", password}, &raw_reply);
    if (!exec_status.ok()) {
        return exec_status;
    }
    RedisReplyGuard reply(raw_reply);
    if (reply->type == REDIS_REPLY_STATUS && reply->str != nullptr &&
        std::strcmp(reply->str, "OK") == 0) {
        return Status::OK();
    }
    if (reply->type == REDIS_REPLY_ERROR) {
        return Status::MetadataError("redis auth failed: " +
                                     std::string(reply->str));
    }
    return Status::MetadataError("redis auth failed: unexpected reply");
}

Status SelectDb(redisContext* ctx, int db_index) {
    redisReply* raw_reply = nullptr;
    Status exec_status =
        ExecCommandArgv(ctx, {"SELECT", std::to_string(db_index)}, &raw_reply);
    if (!exec_status.ok()) {
        return exec_status;
    }
    RedisReplyGuard reply(raw_reply);
    if (reply->type == REDIS_REPLY_STATUS && reply->str != nullptr &&
        std::strcmp(reply->str, "OK") == 0) {
        return Status::OK();
    }
    if (reply->type == REDIS_REPLY_ERROR) {
        return Status::MetadataError("redis select failed: " +
                                     std::string(reply->str));
    }
    return Status::MetadataError("redis select failed: unexpected reply");
}

Result<std::string> MapQueryItem(redisReply* reply) {
    if (reply->type == REDIS_REPLY_STRING) {
        return {Status::OK(), std::string(reply->str, reply->len)};
    }
    if (reply->type == REDIS_REPLY_NIL) {
        return {Status::NotFound("key not found"), ""};
    }
    if (reply->type == REDIS_REPLY_ERROR) {
        return {Status::MetadataError(reply->str == nullptr ? "redis query error"
                                                            : reply->str),
                ""};
    }
    return {Status::Internal("unexpected MGET item"), ""};
}

Status MapSetReply(redisReply* reply, bool insert_mode) {
    if (reply->type == REDIS_REPLY_STATUS && reply->str != nullptr &&
        std::strcmp(reply->str, "OK") == 0) {
        return Status::OK();
    }
    if (reply->type == REDIS_REPLY_NIL) {
        return insert_mode ? Status::AlreadyExists("key already exists")
                           : Status::NotFound("key not found");
    }
    if (reply->type == REDIS_REPLY_ERROR) {
        return Status::MetadataError(reply->str == nullptr ? "redis write error"
                                                           : reply->str);
    }
    return Status::Internal("unexpected SET reply");
}

Status MapDeleteReply(redisReply* reply) {
    if (reply->type == REDIS_REPLY_INTEGER) {
        return Status::OK();
    }
    if (reply->type == REDIS_REPLY_ERROR) {
        return Status::MetadataError(reply->str == nullptr ? "redis delete error"
                                                           : reply->str);
    }
    return Status::Internal("unexpected DEL reply");
}

}  // namespace

RedisMetaStoreBackend::RedisMetaStoreBackend() = default;

RedisMetaStoreBackend::~RedisMetaStoreBackend() {
    (void)reset();
}

Status RedisMetaStoreBackend::init(const MetaStoreInitOptions& options) {
    if (options.backend != BackendKind::kRedis) {
        return Status::InvalidArgument("redis adapter only supports redis backend");
    }
    if (options.endpoint.empty()) {
        return Status::InvalidArgument("endpoint is empty");
    }
    if (options.db_index < 0) {
        return Status::InvalidArgument("db_index must be non-negative");
    }
    if (initialized_) {
        return Status::MetadataError("adapter already initialized");
    }

    Status connect_status = Status::MetadataError("connect path not attempted");
    const bool prefer_local_socket =
        options.prefer_local_socket && !options.unix_socket_path.empty() &&
        IsLocalRedisEndpoint(options.endpoint);

    if (prefer_local_socket) {
        connect_status = ConnectViaUnixSocket(options.unix_socket_path, &redis_ctx_);
        if (!connect_status.ok()) {
            if (redis_ctx_ != nullptr) {
                redisFree(redis_ctx_);
                redis_ctx_ = nullptr;
            }
            connect_status = ConnectViaTcp(options.endpoint, &redis_ctx_);
        }
    } else {
        connect_status = ConnectViaTcp(options.endpoint, &redis_ctx_);
    }

    if (!connect_status.ok()) {
        return connect_status;
    }

    Status auth_status = AuthenticateIfNeeded(redis_ctx_, options.password);
    if (!auth_status.ok()) {
        (void)reset();
        return auth_status;
    }

    Status select_status = SelectDb(redis_ctx_, options.db_index);
    if (!select_status.ok()) {
        (void)reset();
        return select_status;
    }

    options_ = options;
    initialized_ = true;
    return Status::OK();
}

Status RedisMetaStoreBackend::reset() {
    initialized_ = false;
    options_ = MetaStoreInitOptions();
    if (redis_ctx_ != nullptr) {
        redisFree(redis_ctx_);
        redis_ctx_ = nullptr;
    }
    return Status::OK();
}

std::vector<Result<std::string>> RedisMetaStoreBackend::batchQuery(
    const std::vector<std::string>& keys) {
    if (keys.empty()) {
        return {};
    }

    Status validation = validateKeys(keys);
    if (!validation.ok()) {
        return makeErrorResults(keys.size(), validation);
    }

    std::vector<std::string> parts = {"MGET"};
    parts.insert(parts.end(), keys.begin(), keys.end());

    redisReply* raw_reply = nullptr;
    Status exec_status = ExecCommandArgv(redis_ctx_, parts, &raw_reply);
    if (!exec_status.ok()) {
        return makeErrorResults(keys.size(), exec_status);
    }
    RedisReplyGuard reply(raw_reply);

    if (reply->type != REDIS_REPLY_ARRAY ||
        static_cast<std::size_t>(reply->elements) != keys.size()) {
        return makeErrorResults(keys.size(),
                                Status::Internal("unexpected MGET reply"));
    }

    std::vector<Result<std::string>> results;
    results.reserve(keys.size());
    for (std::size_t i = 0; i < keys.size(); ++i) {
        results.push_back(MapQueryItem(reply->element[i]));
    }
    return results;
}

std::vector<Status> RedisMetaStoreBackend::batchInsert(
    const std::vector<std::string>& keys,
    const std::vector<std::string>& values) {
    if (keys.empty() && values.empty()) {
        return {};
    }

    Status validation = validateKeyValues(keys, values);
    if (!validation.ok()) {
        return makeErrorStatuses(keys.size(), validation);
    }

    for (std::size_t i = 0; i < keys.size(); ++i) {
        Status append_status =
            AppendCommandArgv(redis_ctx_, {"SET", keys[i], values[i], "NX"});
        if (!append_status.ok()) {
            return makeErrorStatuses(keys.size(), append_status);
        }
    }

    std::vector<Status> statuses;
    statuses.reserve(keys.size());
    for (std::size_t i = 0; i < keys.size(); ++i) {
        redisReply* raw_reply = nullptr;
        Status reply_status = FetchPipelineReply(redis_ctx_, &raw_reply);
        if (!reply_status.ok()) {
            return makeErrorStatuses(keys.size(), reply_status);
        }
        RedisReplyGuard reply(raw_reply);
        statuses.push_back(MapSetReply(reply.get(), true));
    }
    return statuses;
}

std::vector<Status> RedisMetaStoreBackend::batchUpdate(
    const std::vector<std::string>& keys,
    const std::vector<std::string>& values) {
    if (keys.empty() && values.empty()) {
        return {};
    }

    Status validation = validateKeyValues(keys, values);
    if (!validation.ok()) {
        return makeErrorStatuses(keys.size(), validation);
    }

    for (std::size_t i = 0; i < keys.size(); ++i) {
        Status append_status =
            AppendCommandArgv(redis_ctx_, {"SET", keys[i], values[i], "XX"});
        if (!append_status.ok()) {
            return makeErrorStatuses(keys.size(), append_status);
        }
    }

    std::vector<Status> statuses;
    statuses.reserve(keys.size());
    for (std::size_t i = 0; i < keys.size(); ++i) {
        redisReply* raw_reply = nullptr;
        Status reply_status = FetchPipelineReply(redis_ctx_, &raw_reply);
        if (!reply_status.ok()) {
            return makeErrorStatuses(keys.size(), reply_status);
        }
        RedisReplyGuard reply(raw_reply);
        statuses.push_back(MapSetReply(reply.get(), false));
    }
    return statuses;
}

std::vector<Status> RedisMetaStoreBackend::batchDelete(
    const std::vector<std::string>& keys) {
    if (keys.empty()) {
        return {};
    }

    Status validation = validateKeys(keys);
    if (!validation.ok()) {
        return makeErrorStatuses(keys.size(), validation);
    }

    for (const std::string& key : keys) {
        Status append_status = AppendCommandArgv(redis_ctx_, {"DEL", key});
        if (!append_status.ok()) {
            return makeErrorStatuses(keys.size(), append_status);
        }
    }

    std::vector<Status> statuses;
    statuses.reserve(keys.size());
    for (std::size_t i = 0; i < keys.size(); ++i) {
        redisReply* raw_reply = nullptr;
        Status reply_status = FetchPipelineReply(redis_ctx_, &raw_reply);
        if (!reply_status.ok()) {
            return makeErrorStatuses(keys.size(), reply_status);
        }
        RedisReplyGuard reply(raw_reply);
        statuses.push_back(MapDeleteReply(reply.get()));
    }
    return statuses;
}

Status RedisMetaStoreBackend::clear() {
    Status connected = validateConnected();
    if (!connected.ok()) {
        return connected;
    }
    if (!options_.enable_clear) {
        return Status::NotSupported("clear is disabled");
    }
    if (!options_.db_exclusive) {
        return Status::NotSupported("clear requires db_exclusive");
    }

    redisReply* raw_reply = nullptr;
    Status exec_status = ExecCommandArgv(redis_ctx_, {"FLUSHDB"}, &raw_reply);
    if (!exec_status.ok()) {
        return exec_status;
    }
    RedisReplyGuard reply(raw_reply);
    if (reply->type == REDIS_REPLY_STATUS && reply->str != nullptr &&
        std::strcmp(reply->str, "OK") == 0) {
        return Status::OK();
    }
    if (reply->type == REDIS_REPLY_ERROR) {
        return Status::MetadataError(reply->str == nullptr ? "redis clear error"
                                                           : reply->str);
    }
    return Status::Internal("unexpected FLUSHDB reply");
}

Status RedisMetaStoreBackend::validateConnected() const {
    if (!initialized_) {
        return Status::MetadataError("adapter is not initialized");
    }
    if (redis_ctx_ == nullptr || redis_ctx_->err != 0) {
        return Status::MetadataError("redis connection is not available");
    }
    return Status::OK();
}

Status RedisMetaStoreBackend::validateKeys(
    const std::vector<std::string>& keys) const {
    Status connected = validateConnected();
    if (!connected.ok()) {
        return connected;
    }
    for (const std::string& key : keys) {
        if (key.empty()) {
            return Status::InvalidArgument("key must not be empty");
        }
    }
    return Status::OK();
}

Status RedisMetaStoreBackend::validateKeyValues(
    const std::vector<std::string>& keys,
    const std::vector<std::string>& values) const {
    if (keys.size() != values.size()) {
        return Status::InvalidArgument("keys and values size mismatch");
    }
    return validateKeys(keys);
}

std::vector<Status> RedisMetaStoreBackend::makeErrorStatuses(
    std::size_t count, const Status& status) const {
    return std::vector<Status>(count, status);
}

std::vector<Result<std::string>> RedisMetaStoreBackend::makeErrorResults(
    std::size_t count, const Status& status) const {
    return std::vector<Result<std::string>>(count, Result<std::string>{status, ""});
}

}  // namespace metastore
