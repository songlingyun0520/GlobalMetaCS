#pragma once

/*
 * hi_index.h
 *
 * Abstract interface for HiIndex batch operations.
 * HiIndex resolves index entries into one of:
 *   - direct address
 *   - owner IP
 *   - null / miss
 */

#include "gmm_types.h"

#include <cstdint>
#include <string>
#include <vector>

using RequestID = std::uint64_t;
using LayerID = std::uint32_t;
using IndexValue = std::string;
using TokenKey = std::string;
using BlockKey = std::string;
using Address = std::string;
using IpAddress = std::string;

enum class IndexQueryKind {
    kAddress = 0,
    kIp = 1,
    kNull = 2,
};

struct IndexQueryResult {
    IndexQueryKind kind = IndexQueryKind::kNull;
    std::string value;
};

class HiIndex {
public:
    virtual ~HiIndex() = default;

    virtual std::vector<IndexQueryResult> BatchQueryIndex(
        RequestID request_id,
        LayerID layer_id,
        const std::vector<IndexValue>& index_list) const = 0;

    virtual std::vector<Status> BatchInsertIndex(
        RequestID request_id,
        LayerID layer_id,
        const std::vector<IndexValue>& index_list,
        const std::vector<Address>& address_list) = 0;

    virtual std::vector<Status> BatchUpdateIndex(
        RequestID request_id,
        LayerID layer_id,
        const std::vector<IndexValue>& index_list,
        const std::vector<Address>& address_list) = 0;

    virtual std::vector<Status> BatchDeleteIndex(
        RequestID request_id,
        LayerID layer_id,
        const std::vector<IndexValue>& index_list) = 0;
};
