#include "core/services/EconomicDataIngestionService.h"

namespace oqt::services {

EconomicDataIngestionService::EconomicDataIngestionService(
    oqt::storage::SqliteCanonicalDatasetStore& store)
    : m_store(store) {
}

bool EconomicDataIngestionService::persist(
    const oqt::providers::AdapterResult& result,
    QString* errorMessage) {
    if (!result.isValid()) {
        if (errorMessage) {
            *errorMessage = result.errors.isEmpty()
                ? QStringLiteral("provider adapter result is invalid")
                : result.errors.join(QStringLiteral("; "));
        }
        return false;
    }
    if (!result.dataset.has_value()) {
        if (errorMessage) *errorMessage = QStringLiteral("adapter result does not contain an economic dataset");
        return false;
    }
    return m_store.upsertDataset(*result.dataset, &result.rawResult, errorMessage);
}

} // namespace oqt::services
