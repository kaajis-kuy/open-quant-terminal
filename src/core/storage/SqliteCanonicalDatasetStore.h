#pragma once

#include "core/data/CanonicalData.h"

#include <QSqlDatabase>
#include <QString>

#include <optional>

namespace oqt::storage {

struct CanonicalDatasetPage {
    oqt::data::CanonicalDataset dataset;
    bool hasMore = false;
    qint64 offset = 0;
    qsizetype pageSize = 0;
};

// SQLite persistence boundary for canonical economic observations.
// Dataset metadata and observations are committed atomically.
class SqliteCanonicalDatasetStore final {
public:
    explicit SqliteCanonicalDatasetStore(QString databasePath);
    ~SqliteCanonicalDatasetStore();

    SqliteCanonicalDatasetStore(const SqliteCanonicalDatasetStore&) = delete;
    SqliteCanonicalDatasetStore& operator=(const SqliteCanonicalDatasetStore&) = delete;

    [[nodiscard]] bool open(QString* errorMessage = nullptr);
    [[nodiscard]] bool isOpen() const;
    // Checks SQLite page integrity and declared foreign-key relationships.
    [[nodiscard]] bool verifyIntegrity(QString* errorMessage = nullptr) const;
    // Creates a consistent, compact SQLite snapshot at a new destination path.
    [[nodiscard]] bool backupTo(const QString& destinationPath,
                                QString* errorMessage = nullptr) const;

    // Upserts dataset metadata and observations without deleting older periods.
    [[nodiscard]] bool upsertDataset(
        const oqt::data::CanonicalDataset& dataset,
        const oqt::data::ProviderRawResult* rawResult = nullptr,
        QString* errorMessage = nullptr);

    [[nodiscard]] std::optional<oqt::data::CanonicalDataset> loadDataset(
        const QString& providerId,
        const QString& datasetId,
        QString* errorMessage = nullptr) const;
    // Returns one deterministic SQL page ordered by series, period and key fields.
    [[nodiscard]] std::optional<CanonicalDatasetPage> loadDatasetPage(
        const QString& providerId, const QString& datasetId,
        qsizetype pageSize, qint64 offset = 0, QString* errorMessage = nullptr) const;

private:
    void closeConnection();
    [[nodiscard]] std::optional<oqt::data::CanonicalDataset> loadDatasetImpl(
        const QString& providerId, const QString& datasetId, qsizetype pageSize,
        qint64 offset, QString* errorMessage, bool* hasMore) const;
    [[nodiscard]] bool initializeSchema(QString* errorMessage);

    QString m_databasePath;
    QString m_connectionName;
    QSqlDatabase m_database;
};

} // namespace oqt::storage
