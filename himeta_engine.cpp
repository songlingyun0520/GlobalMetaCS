#include "himeta_engine.h"

#include <stdexcept>

HiMetaEngine::HiMetaEngine(HiIndex& hi_index,
                           LocalMetaManagement& local_meta_management,
                           GlobalMetaClient* global_meta_client)
    : hi_index_(hi_index),
      local_meta_management_(local_meta_management),
      global_meta_client_(global_meta_client) {}

HiIndex& HiMetaEngine::hiIndex() {
    return hi_index_;
}

const HiIndex& HiMetaEngine::hiIndex() const {
    return hi_index_;
}

GlobalMetaClient& HiMetaEngine::globalMetaClient() {
    if (global_meta_client_ == nullptr) {
        throw std::logic_error("GlobalMetaClient is not configured");
    }
    return *global_meta_client_;
}

const GlobalMetaClient& HiMetaEngine::globalMetaClient() const {
    if (global_meta_client_ == nullptr) {
        throw std::logic_error("GlobalMetaClient is not configured");
    }
    return *global_meta_client_;
}

bool HiMetaEngine::hasGlobalMetaClient() const {
    return global_meta_client_ != nullptr;
}

LocalMetaManagement& HiMetaEngine::localMetaManagement() {
    return local_meta_management_;
}

const LocalMetaManagement& HiMetaEngine::localMetaManagement() const {
    return local_meta_management_;
}
