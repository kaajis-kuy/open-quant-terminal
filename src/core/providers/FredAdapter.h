#pragma once

#include "core/providers/ProviderAdapter.h"

namespace oqt::providers {

class FredAdapter final : public ProviderAdapter {
public:
    [[nodiscard]] QString providerId() const override;
    [[nodiscard]] QStringList supportedOperations() const override;

    [[nodiscard]] AdapterResult normalize(
        const ProviderResponse& response,
        const ProviderRequestContext& context) const override;
};

} // namespace oqt::providers
