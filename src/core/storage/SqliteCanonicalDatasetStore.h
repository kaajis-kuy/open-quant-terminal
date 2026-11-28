#pragma once

#include "core/data/CanonicalData.h"

#include <QSqlDatabase>
#include <QString>

#include <optional>

namespace oqt::storage {

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

    // Upserts dataset metadata and observations without deleting older periods.
    [[nodiscard]] bool upsertDataset(
        const oqt::data::CanonicalDataset& dataset,
        const oqt::data::ProviderRawResult* rawResult = nullptr,
        QString* errorMessage = nullptr);

    [[nodiscard]] std::optional<oqt::data::CanonicalDataset> loadDataset(
        const QString& providerId,
        const QString& datasetId,
        QString* errorMessage = nullptr) const;

private:
    void closeConnection();
    [[nodiscard]] bool initializeSchema(QString* errorMessage);

    QString m_databasePath;
    QString m_connectionName;
    QSqlDatabase m_database;
};

} // namespace oqt::storage
