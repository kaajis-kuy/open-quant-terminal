#pragma once

#include "core/providers/ProviderAdapter.h"
#include "core/storage/SqliteMarketDataStore.h"

#include <QString>

namespace oqt::services {

enum class MarketDataWriteMode {
    Replace,
    Upsert
};

// Provider-neutral boundary between normalized provider results and OHLCV storage.
class MarketDataIngestionService final {
public:
    explicit MarketDataIngestionService(oqt::storage::SqliteMarketDataStore& store);

    // Persists only valid canonical market-series results. Scalar datasets are rejected.
    [[nodiscard]] bool persist(const oqt::providers::AdapterResult& result,
                               MarketDataWriteMode mode,
                               QString* errorMessage = nullptr);

private:
    oqt::storage::SqliteMarketDataStore& m_store;
};

} // namespace oqt::services
