#include "core/services/MarketDataIngestionService.h"

namespace oqt::services {

MarketDataIngestionService::MarketDataIngestionService(
    oqt::storage::SqliteMarketDataStore& store)
    : m_store(store) {
}

bool MarketDataIngestionService::persist(
    const oqt::providers::AdapterResult& result,
    MarketDataWriteMode mode,
    QString* errorMessage) {
    const auto fail = [errorMessage](const QString& message) {
        if (errorMessage) {
            *errorMessage = message;
        }
        return false;
    };

    if (!m_store.isOpen()) {
        return fail(QStringLiteral("Market data store is not open"));
    }
    if (!result.isValid()) {
        return fail(QStringLiteral("Provider adapter result is invalid"));
    }
    if (!result.marketSeries.has_value() || result.dataset.has_value()) {
        return fail(QStringLiteral("Provider result does not contain a canonical market series"));
    }

    QString storageError;
    bool saved = false;
    switch (mode) {
    case MarketDataWriteMode::Replace:
        saved = m_store.saveSeries(*result.marketSeries, &storageError);
        break;
    case MarketDataWriteMode::Upsert:
        saved = m_store.upsertSeries(*result.marketSeries, &storageError);
        break;
    default:
        return fail(QStringLiteral("Unsupported market data write mode"));
    }

    if (!saved) {
        return fail(storageError.isEmpty()
            ? QStringLiteral("Failed to persist canonical market series")
            : storageError);
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

} // namespace oqt::services
