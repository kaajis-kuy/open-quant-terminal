#pragma once

#include "core/data/MarketData.h"
#include "core/data/CatalogData.h"

#include <QSqlDatabase>
#include <QString>

#include <optional>

namespace oqt::storage {

struct CanonicalMarketSeriesPage {
    oqt::data::CanonicalMarketSeries series;
    bool hasMore = false;
    qint64 offset = 0;
    qsizetype pageSize = 0;
};

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
    // Checks SQLite page integrity and declared foreign-key relationships.
    [[nodiscard]] bool verifyIntegrity(QString* errorMessage = nullptr) const;
    // Creates a consistent, compact SQLite snapshot at a new destination path.
    [[nodiscard]] bool backupTo(const QString& destinationPath,
                                 QString* errorMessage = nullptr) const;
    [[nodiscard]] bool saveSeries(const oqt::data::CanonicalMarketSeries& series,
                                  QString* errorMessage = nullptr);
    // Inserts or updates supplied candles without deleting older stored bars.
    [[nodiscard]] bool upsertSeries(const oqt::data::CanonicalMarketSeries& series,
                                    QString* errorMessage = nullptr);
    [[nodiscard]] std::optional<oqt::data::CanonicalMarketSeries> loadSeries(
        const QString& providerId,
        const QString& instrumentId,
        const QString& interval,
        QString* errorMessage = nullptr) const;
    // Returns one deterministic database page ordered by candle timestamp ascending.
    [[nodiscard]] std::optional<CanonicalMarketSeriesPage> loadSeriesPage(
        const QString& providerId, const QString& instrumentId, const QString& interval,
        qsizetype pageSize, qint64 offset = 0, QString* errorMessage = nullptr) const;
    // Returns bars in the half-open UTC interval [startInclusive, endExclusive).
    [[nodiscard]] std::optional<oqt::data::CanonicalMarketSeries> loadSeriesRange(
        const QString& providerId,
        const QString& instrumentId,
        const QString& interval,
        const QDateTime& startInclusive,
        const QDateTime& endExclusive,
        QString* errorMessage = nullptr,
        qsizetype maxBars = 0) const;

    // Lists locally persisted provider/instrument/interval combinations; never contacts providers.
    [[nodiscard]] std::optional<oqt::data::MarketInstrumentCatalogPage> loadInstrumentCatalog(
        const QString& searchText = {}, qsizetype pageSize = 100, qint64 offset = 0,
        QString* errorMessage = nullptr) const;

private:
    [[nodiscard]] std::optional<oqt::data::CanonicalMarketSeries> loadSeriesImpl(
        const QString& providerId,
        const QString& instrumentId,
        const QString& interval,
        std::optional<qint64> startInclusiveMs,
        std::optional<qint64> endExclusiveMs,
        qsizetype maxBars,
        QString* errorMessage,
        qint64 offset = 0,
        bool* hasMore = nullptr) const;
    [[nodiscard]] bool initializeSchema(QString* errorMessage);

    QString m_databasePath;
    QString m_connectionName;
    QSqlDatabase m_database;
};

} // namespace oqt::storage
