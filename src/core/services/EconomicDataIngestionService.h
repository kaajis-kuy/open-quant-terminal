#pragma once

#include "core/providers/ProviderAdapter.h"
#include "core/storage/SqliteCanonicalDatasetStore.h"

#include <QString>

namespace oqt::services {

// Provider-neutral boundary between canonical economic datasets and SQLite.
class EconomicDataIngestionService final {
public:
    explicit EconomicDataIngestionService(
        oqt::storage::SqliteCanonicalDatasetStore& store);

    [[nodiscard]] bool persist(
        const oqt::providers::AdapterResult& result,
        QString* errorMessage = nullptr);

private:
    oqt::storage::SqliteCanonicalDatasetStore& m_store;
};

} // namespace oqt::services
