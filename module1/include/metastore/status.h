#pragma once

#include <string>

namespace metastore {

enum class StatusCode {
    kOk = 0,
    kInvalidArgument,
    kNotFound,
    kAlreadyExists,
    kNotSupported,
    kMetadataError,
    kInternal,
};

class Status {
public:
    Status() : code_(StatusCode::kOk) {}
    Status(StatusCode code, std::string message)
        : code_(code), message_(std::move(message)) {}

    static Status OK() { return Status(); }
    static Status InvalidArgument(const std::string& message) {
        return Status(StatusCode::kInvalidArgument, message);
    }
    static Status NotFound(const std::string& message) {
        return Status(StatusCode::kNotFound, message);
    }
    static Status AlreadyExists(const std::string& message) {
        return Status(StatusCode::kAlreadyExists, message);
    }
    static Status NotSupported(const std::string& message) {
        return Status(StatusCode::kNotSupported, message);
    }
    static Status MetadataError(const std::string& message) {
        return Status(StatusCode::kMetadataError, message);
    }
    static Status Internal(const std::string& message) {
        return Status(StatusCode::kInternal, message);
    }

    bool ok() const { return code_ == StatusCode::kOk; }
    bool IsInvalidArgument() const { return code_ == StatusCode::kInvalidArgument; }
    bool IsNotFound() const { return code_ == StatusCode::kNotFound; }
    bool IsAlreadyExists() const { return code_ == StatusCode::kAlreadyExists; }
    bool IsNotSupported() const { return code_ == StatusCode::kNotSupported; }
    bool IsMetadataError() const { return code_ == StatusCode::kMetadataError; }
    bool IsInternal() const { return code_ == StatusCode::kInternal; }

    StatusCode code() const { return code_; }
    const std::string& message() const { return message_; }

private:
    StatusCode code_;
    std::string message_;
};

}  // namespace metastore
