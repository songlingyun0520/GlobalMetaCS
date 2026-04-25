#pragma once

/*
 * himeta_engine.h
 *
 * HiMetaEngine only wraps and exposes three capability groups directly:
 *   - HiIndex
 *   - GlobalMetaClient
 *   - LocalMetaManagement
 */

#include "hi_index.h"
#include "local_meta_management.h"

#include <string>
#include <vector>

class GlobalMetaClient;

class HiMetaEngine {
public:
    HiMetaEngine(HiIndex& hi_index,
                 LocalMetaManagement& local_meta_management,
                 GlobalMetaClient* global_meta_client = nullptr);

    HiIndex& hiIndex();
    const HiIndex& hiIndex() const;

    GlobalMetaClient& globalMetaClient();
    const GlobalMetaClient& globalMetaClient() const;
    bool hasGlobalMetaClient() const;

    LocalMetaManagement& localMetaManagement();
    const LocalMetaManagement& localMetaManagement() const;

private:
    HiIndex& hi_index_;
    LocalMetaManagement& local_meta_management_;
    GlobalMetaClient* global_meta_client_;
};
