#pragma once

/*
 * gmm_types.h
 *
 * Shared types for the GlobalMetaManagement module:
 *   Status    — operation result (ok / error + message)
 *   Result<T> — value + Status pair
 */

#include <string>

// ---------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------
class Status {
public:
    Status() : ok_(true) {}

    static Status error(const std::string& msg) {
        Status s;
        s.ok_      = false;
        s.message_ = msg;
        return s;
    }

    bool               ok()      const { return ok_; }
    const std::string& message() const { return message_; }

private:
    bool        ok_      = true;
    std::string message_;
};

// ---------------------------------------------------------------------------
// Result<T>
// ---------------------------------------------------------------------------
template <typename T>
struct Result {
    bool ok() const { return status.ok(); }
    T      value{};
    Status status = Status::error("not set");
};
