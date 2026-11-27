#pragma once

#include "core/providers/ProviderAdapter.h"

namespace oqt::services {

// Resolves provider adapters and validates the requested operation before normalization.
class ProviderNormalizationService final {
public:
    explicit ProviderNormalizationService(
        const oqt::providers::ProviderAdapterRegistry& registry);

    [[nodiscard]] oqt::providers::AdapterResult normalize(
        const oqt::providers::ProviderResponse& response,
        const oqt::providers::ProviderRequestContext& context) const;

private:
    const oqt::providers::ProviderAdapterRegistry& m_registry;
};

} // namespace oqt::services
