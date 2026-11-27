#pragma once

#include "core/data/MarketData.h"

#include <QSqlDatabase>
#include <QString>

#include <optional>

namespace oqt::storage {

// Small SQLite persistence boundary for canonical OHLCV series.
// A save replaces one complete series atomically; it never merges candles.
class SqliteMarketDataStore final {
public:
    explicit SqliteMarketDataStore(QString databasePath);
    ~SqliteMarketDataStore();

    SqliteMarketDataStore(const SqliteMarketDataStore&) = delete;
    SqliteMarketDataStore& operator=(const SqliteMarketDataStore&) = delete;

    [[nodiscard]] bool open(QString* errorMessage = nullptr);
    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] bool saveSeries(const oqt::data::CanonicalMarketSeries& series,
                                  QString* errorMessage = nullptr);
    [[nodiscard]] std::optional<oqt::data::CanonicalMarketSeries> loadSeries(
        const QString& providerId,
        const QString& instrumentId,
        const QString& interval,
        QString* errorMessage = nullptr) const;

private:
    [[nodiscard]] bool initializeSchema(QString* errorMessage);

    QString m_databasePath;
    QString m_connectionName;
    QSqlDatabase m_database;
};

} // namespace oqt::storage
