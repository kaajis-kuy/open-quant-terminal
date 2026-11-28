#include "core/storage/SqliteCanonicalDatasetStore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <algorithm>
#include <limits>
#include <utility>

namespace oqt::storage {
namespace {

void setError(QString* target, const QString& message) {
    if (target) *target = message;
}

QString sqlError(const QSqlQuery& query) {
    return query.lastError().text();
}

QString nonNullText(const QString& value) {
    return value.isNull() ? QStringLiteral("") : value;
}

QString utcIso(const QDateTime& value) {
    return value.isValid() ? value.toUTC().toString(Qt::ISODateWithMs) : QStringLiteral("");
}

QJsonObject missingCountsToJson(const QHash<QString, qsizetype>& counts) {
    QJsonObject object;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it)
        object.insert(it.key(), static_cast<double>(it.value()));
    return object;
}

QJsonArray stringsToJson(const QStringList& values) {
    QJsonArray array;
    for (const QString& value : values) array.append(value);
    return array;
}

} // namespace

SqliteCanonicalDatasetStore::SqliteCanonicalDatasetStore(QString databasePath)
    : m_databasePath(std::move(databasePath)),
      m_connectionName(QStringLiteral("oqt-economic-store-%1")
          .arg(QUuid::createUuid().toString(QUuid::WithoutBraces))) {
}

SqliteCanonicalDatasetStore::~SqliteCanonicalDatasetStore() {
    closeConnection();
}

void SqliteCanonicalDatasetStore::closeConnection() {
    if (!m_database.isValid()) return;
    m_database.close();
    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool SqliteCanonicalDatasetStore::open(QString* errorMessage) {
    if (isOpen()) return true;
    if (m_databasePath.trimmed().isEmpty()) {
        setError(errorMessage, QStringLiteral("database path is required"));
        return false;
    }

    // A previous failed open must not leave a registered connection behind.
    closeConnection();

    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(m_databasePath);
    if (!m_database.open()) {
        const QString message = m_database.lastError().text();
        setError(errorMessage, message);
        closeConnection();
        return false;
    }

    QString setupError;
    {
        QSqlQuery pragmas(m_database);
        if (!pragmas.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
            setupError = sqlError(pragmas);
        } else {
            pragmas.finish();
            if (!pragmas.exec(QStringLiteral("PRAGMA busy_timeout = 5000"))) {
                setupError = sqlError(pragmas);
            } else {
                pragmas.finish();
                if (m_databasePath != QStringLiteral(":memory:")) {
                    if (!pragmas.exec(QStringLiteral("PRAGMA journal_mode = WAL"))
                        || !pragmas.next()
                        || pragmas.value(0).toString().compare(
                            QStringLiteral("wal"), Qt::CaseInsensitive) != 0) {
                        setupError = pragmas.lastError().isValid()
                            ? sqlError(pragmas)
                            : QStringLiteral("SQLite could not enable WAL mode");
                    }
                    pragmas.finish();
                }
            }
        }
    }

    if (!setupError.isEmpty()) {
        setError(errorMessage, setupError);
        closeConnection();
        return false;
    }

    if (!initializeSchema(errorMessage)) {
        closeConnection();
        return false;
    }
    return true;
}

bool SqliteCanonicalDatasetStore::isOpen() const {
    return m_database.isValid() && m_database.isOpen();
}

bool SqliteCanonicalDatasetStore::verifyIntegrity(QString* errorMessage) const {
    if (!isOpen()) {
        setError(errorMessage, QStringLiteral("database is not open"));
        return false;
    }

    QSqlQuery integrity(m_database);
    if (!integrity.exec(QStringLiteral("PRAGMA integrity_check"))) {
        setError(errorMessage, sqlError(integrity));
        return false;
    }

    bool integrityOk = false;
    QStringList integrityErrors;
    while (integrity.next()) {
        const QString result = integrity.value(0).toString();
        if (result == QStringLiteral("ok")) {
            integrityOk = true;
        } else {
            integrityErrors.append(result);
        }
    }
    integrity.finish();
    if (!integrityOk || !integrityErrors.isEmpty()) {
        setError(errorMessage, integrityErrors.isEmpty()
            ? QStringLiteral("SQLite integrity_check returned no result")
            : integrityErrors.join(QStringLiteral("; ")));
        return false;
    }

    QSqlQuery foreignKeys(m_database);
    if (!foreignKeys.exec(QStringLiteral("PRAGMA foreign_key_check"))) {
        setError(errorMessage, sqlError(foreignKeys));
        return false;
    }
    if (foreignKeys.next()) {
        const QString message = QStringLiteral(
            "SQLite foreign-key violation in table '%1' at row %2 (parent '%3', constraint %4)")
            .arg(foreignKeys.value(0).toString())
            .arg(foreignKeys.value(1).toString())
            .arg(foreignKeys.value(2).toString())
            .arg(foreignKeys.value(3).toString());
        setError(errorMessage, message);
        return false;
    }
    return true;
}

bool SqliteCanonicalDatasetStore::backupTo(
    const QString& destinationPath, QString* errorMessage) const {
    if (!isOpen()) {
        setError(errorMessage, QStringLiteral("database is not open"));
        return false;
    }

    const QString destination = destinationPath.trimmed();
    if (destination.isEmpty()) {
        setError(errorMessage, QStringLiteral("backup destination path is required"));
        return false;
    }

    const QString sourceAbsolute = QFileInfo(m_databasePath).absoluteFilePath();
    const QString destinationAbsolute = QFileInfo(destination).absoluteFilePath();
    if (sourceAbsolute == destinationAbsolute) {
        setError(errorMessage, QStringLiteral("backup destination must differ from source database"));
        return false;
    }
    if (QFile::exists(destinationAbsolute)) {
        setError(errorMessage, QStringLiteral("backup destination already exists"));
        return false;
    }

    // VACUUM INTO creates a consistent snapshot including committed WAL data.
    QSqlQuery backup(m_database);
    if (!backup.prepare(QStringLiteral("VACUUM INTO ?"))) {
        setError(errorMessage, sqlError(backup));
        return false;
    }
    backup.addBindValue(destinationAbsolute);
    if (!backup.exec()) {
        setError(errorMessage, sqlError(backup));
        return false;
    }
    return true;
}

bool SqliteCanonicalDatasetStore::initializeSchema(QString* errorMessage) {
    QSqlQuery version(m_database);
    if (!version.exec(QStringLiteral("PRAGMA user_version")) || !version.next()) {
        setError(errorMessage, sqlError(version));
        return false;
    }
    const int schemaVersion = version.value(0).toInt();
    version.finish();
    if (schemaVersion > 1) {
        setError(errorMessage, QStringLiteral("economic database schema is newer than supported"));
        return false;
    }
    if (schemaVersion == 1) return true;

    if (!m_database.transaction()) {
        setError(errorMessage, m_database.lastError().text());
        return false;
    }

    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS economic_datasets ("
            "provider_id TEXT NOT NULL, dataset_id TEXT NOT NULL, schema_version TEXT NOT NULL,"
            "source_url TEXT NOT NULL, attribution_id TEXT NOT NULL, retrieved_at TEXT NOT NULL,"
            "quality_status INTEGER NOT NULL, is_partial INTEGER NOT NULL, is_truncated INTEGER NOT NULL,"
            "missing_value_counts TEXT NOT NULL, warnings TEXT NOT NULL, next_page_cursor TEXT NOT NULL,"
            "PRIMARY KEY(provider_id,dataset_id))"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS economic_observations ("
            "provider_id TEXT NOT NULL, dataset_id TEXT NOT NULL, series_id TEXT NOT NULL,"
            "period TEXT NOT NULL, value REAL NULL, unit TEXT NOT NULL, frequency TEXT NOT NULL,"
            "geography TEXT NOT NULL, seasonal_adjustment TEXT NOT NULL, measure_type TEXT NOT NULL,"
            "currency TEXT NOT NULL, base_currency TEXT NOT NULL, quote_currency TEXT NOT NULL,"
            "scale_multiplier REAL NOT NULL, denominator TEXT NOT NULL,"
            "PRIMARY KEY(provider_id,dataset_id,series_id,period,unit,frequency,geography,"
            "seasonal_adjustment,measure_type,currency,base_currency,quote_currency,denominator),"
            "FOREIGN KEY(provider_id,dataset_id) REFERENCES economic_datasets(provider_id,dataset_id)"
            " ON DELETE CASCADE)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS provider_raw_results ("
            "provider_id TEXT NOT NULL, request_id TEXT NOT NULL, operation_id TEXT NOT NULL,"
            "schema_version TEXT NOT NULL, fetched_at TEXT NOT NULL, payload_json TEXT NOT NULL,"
            "payload_reference TEXT NOT NULL, redacted_arguments TEXT NOT NULL, error_message TEXT NOT NULL,"
            "PRIMARY KEY(provider_id,request_id))")
    };
    QSqlQuery query(m_database);
    for (const QString& statement : statements) {
        if (!query.exec(statement)) {
            setError(errorMessage, sqlError(query));
            m_database.rollback();
            return false;
        }
    }
    if (!query.exec(QStringLiteral("PRAGMA user_version = 1"))) {
        setError(errorMessage, sqlError(query));
        m_database.rollback();
        return false;
    }
    if (!m_database.commit()) {
        setError(errorMessage, m_database.lastError().text());
        m_database.rollback();
        return false;
    }
    return true;
}

bool SqliteCanonicalDatasetStore::upsertDataset(
    const oqt::data::CanonicalDataset& dataset,
    const oqt::data::ProviderRawResult* rawResult,
    QString* errorMessage) {
    if (!isOpen()) {
        setError(errorMessage, QStringLiteral("database is not open"));
        return false;
    }
    const QStringList errors = dataset.validate();
    if (!errors.isEmpty()) {
        setError(errorMessage, errors.join(QStringLiteral("; ")));
        return false;
    }
    if (rawResult) {
        const QStringList rawErrors = rawResult->validate();
        if (!rawErrors.isEmpty()) {
            setError(errorMessage, rawErrors.join(QStringLiteral("; ")));
            return false;
        }
        if (rawResult->providerId != dataset.providerId) {
            setError(errorMessage, QStringLiteral("raw result providerId must match dataset providerId"));
            return false;
        }
    }
    if (!m_database.transaction()) {
        setError(errorMessage, m_database.lastError().text());
        return false;
    }

    QSqlQuery metadata(m_database);
    metadata.prepare(QStringLiteral(
        "INSERT INTO economic_datasets(provider_id,dataset_id,schema_version,source_url,"
        "attribution_id,retrieved_at,quality_status,is_partial,is_truncated,missing_value_counts,"
        "warnings,next_page_cursor) VALUES(?,?,?,?,?,?,?,?,?,?,?,?) "
        "ON CONFLICT(provider_id,dataset_id) DO UPDATE SET schema_version=excluded.schema_version,"
        "source_url=excluded.source_url,attribution_id=excluded.attribution_id,"
        "retrieved_at=excluded.retrieved_at,quality_status=excluded.quality_status,"
        "is_partial=excluded.is_partial,is_truncated=excluded.is_truncated,"
        "missing_value_counts=excluded.missing_value_counts,warnings=excluded.warnings,"
        "next_page_cursor=excluded.next_page_cursor"));
    metadata.addBindValue(dataset.providerId);
    metadata.addBindValue(dataset.datasetId);
    metadata.addBindValue(dataset.schemaVersion);
    metadata.addBindValue(nonNullText(dataset.provenance.sourceUrl));
    metadata.addBindValue(nonNullText(dataset.provenance.attributionId));
    metadata.addBindValue(utcIso(dataset.provenance.retrievedAt));
    metadata.addBindValue(static_cast<int>(dataset.quality.status));
    metadata.addBindValue(dataset.quality.isPartial ? 1 : 0);
    metadata.addBindValue(dataset.quality.isTruncated ? 1 : 0);
    metadata.addBindValue(QString::fromUtf8(QJsonDocument(
        missingCountsToJson(dataset.quality.missingValueCounts)).toJson(QJsonDocument::Compact)));
    metadata.addBindValue(QString::fromUtf8(QJsonDocument(
        stringsToJson(dataset.quality.warnings)).toJson(QJsonDocument::Compact)));
    // A default-constructed QString is null; persist an absent cursor as empty TEXT,
    // not SQL NULL, because next_page_cursor is declared NOT NULL.
    metadata.addBindValue(nonNullText(dataset.quality.nextPageCursor));
    if (!metadata.exec()) {
        setError(errorMessage, sqlError(metadata));
        m_database.rollback();
        return false;
    }

    QSqlQuery observation(m_database);
    observation.prepare(QStringLiteral(
        "INSERT INTO economic_observations(provider_id,dataset_id,series_id,period,value,unit,"
        "frequency,geography,seasonal_adjustment,measure_type,currency,base_currency,quote_currency,"
        "scale_multiplier,denominator) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) "
        "ON CONFLICT(provider_id,dataset_id,series_id,period,unit,frequency,geography,"
        "seasonal_adjustment,measure_type,currency,base_currency,quote_currency,denominator) "
        "DO UPDATE SET value=excluded.value,scale_multiplier=excluded.scale_multiplier"));
    for (const auto& item : dataset.observations) {
        observation.bindValue(0, item.providerId);
        observation.bindValue(1, item.datasetId);
        observation.bindValue(2, item.seriesId);
        observation.bindValue(3, item.period.toString(Qt::ISODate));
        observation.bindValue(4, item.value.has_value() ? QVariant(*item.value) : QVariant());
        observation.bindValue(5, item.unit);
        observation.bindValue(6, nonNullText(item.frequency));
        observation.bindValue(7, nonNullText(item.geography));
        observation.bindValue(8, nonNullText(item.seasonalAdjustment));
        observation.bindValue(9, nonNullText(item.measureType));
        observation.bindValue(10, nonNullText(item.currency));
        observation.bindValue(11, nonNullText(item.baseCurrency));
        observation.bindValue(12, nonNullText(item.quoteCurrency));
        observation.bindValue(13, item.scaleMultiplier);
        observation.bindValue(14, nonNullText(item.denominator));
        if (!observation.exec()) {
            setError(errorMessage, sqlError(observation));
            m_database.rollback();
            return false;
        }
        observation.finish();
    }

    if (rawResult) {
        QJsonDocument payload;
        if (rawResult->rawPayload.isObject())
            payload = QJsonDocument(rawResult->rawPayload.toObject());
        else if (rawResult->rawPayload.isArray())
            payload = QJsonDocument(rawResult->rawPayload.toArray());
        else {
            setError(errorMessage, QStringLiteral("raw payload must be a JSON object or array"));
            m_database.rollback();
            return false;
        }
        QSqlQuery raw(m_database);
        raw.prepare(QStringLiteral(
            "INSERT INTO provider_raw_results(provider_id,request_id,operation_id,schema_version,"
            "fetched_at,payload_json,payload_reference,redacted_arguments,error_message)"
            " VALUES(?,?,?,?,?,?,?,?,?) ON CONFLICT(provider_id,request_id) DO UPDATE SET "
            "operation_id=excluded.operation_id,schema_version=excluded.schema_version,"
            "fetched_at=excluded.fetched_at,payload_json=excluded.payload_json,"
            "payload_reference=excluded.payload_reference,redacted_arguments=excluded.redacted_arguments,"
            "error_message=excluded.error_message"));
        raw.addBindValue(rawResult->providerId);
        raw.addBindValue(rawResult->requestId);
        raw.addBindValue(rawResult->operationId);
        raw.addBindValue(rawResult->responseSchemaVersion);
        raw.addBindValue(utcIso(rawResult->fetchedAt));
        raw.addBindValue(QString::fromUtf8(payload.toJson(QJsonDocument::Compact)));
        raw.addBindValue(nonNullText(rawResult->rawPayloadReference));
        raw.addBindValue(rawResult->redactedRequestArguments.join(QLatin1Char('\n')));
        raw.addBindValue(nonNullText(rawResult->errorMessage));
        if (!raw.exec()) {
            setError(errorMessage, sqlError(raw));
            m_database.rollback();
            return false;
        }
    }

    if (!m_database.commit()) {
        setError(errorMessage, m_database.lastError().text());
        m_database.rollback();
        return false;
    }
    return true;
}

std::optional<oqt::data::CanonicalDataset> SqliteCanonicalDatasetStore::loadDataset(
    const QString& providerId, const QString& datasetId, QString* errorMessage) const {
    return loadDatasetImpl(providerId, datasetId, 0, 0, errorMessage, nullptr);
}

std::optional<CanonicalDatasetPage> SqliteCanonicalDatasetStore::loadDatasetPage(
    const QString& providerId, const QString& datasetId, qsizetype pageSize,
    qint64 offset, QString* errorMessage) const {
    constexpr qsizetype kMaximumPageSize = 5000;
    if (pageSize <= 0 || pageSize > kMaximumPageSize) {
        setError(errorMessage, QStringLiteral("pageSize must be between 1 and 5000"));
        return std::nullopt;
    }
    if (offset < 0 || offset > std::numeric_limits<qint64>::max() - pageSize - 1) {
        setError(errorMessage, QStringLiteral("offset is outside the supported range"));
        return std::nullopt;
    }
    bool hasMore = false;
    auto dataset = loadDatasetImpl(providerId, datasetId, pageSize, offset,
                                   errorMessage, &hasMore);
    if (!dataset) return std::nullopt;
    return CanonicalDatasetPage{std::move(*dataset), hasMore, offset, pageSize};
}

std::optional<oqt::data::CanonicalDataset> SqliteCanonicalDatasetStore::loadDatasetImpl(
    const QString& providerId, const QString& datasetId, qsizetype pageSize,
    qint64 offset, QString* errorMessage, bool* hasMore) const {
    if (hasMore) *hasMore = false;
    if (!isOpen()) {
        setError(errorMessage, QStringLiteral("database is not open"));
        return std::nullopt;
    }
    QSqlQuery metadata(m_database);
    metadata.prepare(QStringLiteral(
        "SELECT schema_version,source_url,attribution_id,retrieved_at,quality_status,is_partial,"
        "is_truncated,missing_value_counts,warnings,next_page_cursor FROM economic_datasets "
        "WHERE provider_id=? AND dataset_id=?"));
    metadata.addBindValue(providerId);
    metadata.addBindValue(datasetId);
    if (!metadata.exec()) {
        setError(errorMessage, sqlError(metadata));
        return std::nullopt;
    }
    if (!metadata.next()) return std::nullopt;

    oqt::data::CanonicalDataset dataset;
    dataset.providerId = providerId;
    dataset.datasetId = datasetId;
    dataset.schemaVersion = metadata.value(0).toString();
    dataset.provenance.sourceUrl = metadata.value(1).toString();
    dataset.provenance.attributionId = metadata.value(2).toString();
    dataset.provenance.retrievedAt = QDateTime::fromString(metadata.value(3).toString(), Qt::ISODateWithMs);
    dataset.quality.status = static_cast<oqt::data::DataStatus>(metadata.value(4).toInt());
    dataset.quality.isPartial = metadata.value(5).toBool();
    dataset.quality.isTruncated = metadata.value(6).toBool();
    const auto missing = QJsonDocument::fromJson(metadata.value(7).toString().toUtf8()).object();
    for (auto it = missing.begin(); it != missing.end(); ++it)
        dataset.quality.missingValueCounts.insert(it.key(), static_cast<qsizetype>(it.value().toDouble()));
    const auto warnings = QJsonDocument::fromJson(metadata.value(8).toString().toUtf8()).array();
    for (const auto& warning : warnings)
        if (warning.isString()) dataset.quality.warnings.append(warning.toString());
    dataset.quality.nextPageCursor = metadata.value(9).toString();
    metadata.finish();

    QSqlQuery observations(m_database);
    QString observationsSql = QStringLiteral(
        "SELECT series_id,period,value,unit,frequency,geography,seasonal_adjustment,measure_type,"
        "currency,base_currency,quote_currency,scale_multiplier,denominator "
        "FROM economic_observations WHERE provider_id=? AND dataset_id=? "
        "ORDER BY series_id DESC,period DESC,unit DESC,frequency DESC,geography DESC,"
        "seasonal_adjustment DESC,measure_type DESC,currency DESC,base_currency DESC,"
        "quote_currency DESC,denominator DESC");
    if (pageSize > 0) observationsSql += QStringLiteral(" LIMIT ? OFFSET ?");
    observations.prepare(observationsSql);
    observations.addBindValue(providerId);
    observations.addBindValue(datasetId);
    if (pageSize > 0) {
        observations.addBindValue(pageSize + 1);
        observations.addBindValue(offset);
    }
    if (!observations.exec()) {
        setError(errorMessage, sqlError(observations));
        return std::nullopt;
    }
    while (observations.next()) {
        oqt::data::CanonicalObservation item;
        item.providerId = providerId;
        item.datasetId = datasetId;
        item.seriesId = observations.value(0).toString();
        item.period = QDate::fromString(observations.value(1).toString(), Qt::ISODate);
        if (!observations.isNull(2)) item.value = observations.value(2).toDouble();
        item.unit = observations.value(3).toString();
        item.frequency = observations.value(4).toString();
        item.geography = observations.value(5).toString();
        item.seasonalAdjustment = observations.value(6).toString();
        item.measureType = observations.value(7).toString();
        item.currency = observations.value(8).toString();
        item.baseCurrency = observations.value(9).toString();
        item.quoteCurrency = observations.value(10).toString();
        item.scaleMultiplier = observations.value(11).toDouble();
        item.denominator = observations.value(12).toString();
        dataset.observations.append(std::move(item));
    }
    if (!observations.lastError().text().isEmpty()) {
        setError(errorMessage, sqlError(observations));
        return std::nullopt;
    }
    if (pageSize > 0 && dataset.observations.size() > pageSize) {
        if (hasMore) *hasMore = true;
        dataset.observations.removeLast();
    }

    // Start with the newest stable-key slice, then restore ascending order within
    // each returned page for downstream tables and chart series.
    std::reverse(dataset.observations.begin(), dataset.observations.end());

    const QStringList validationErrors = dataset.validate();
    if (!validationErrors.isEmpty()) {
        setError(errorMessage, validationErrors.join(QStringLiteral("; ")));
        return std::nullopt;
    }
    return dataset;
}

} // namespace oqt::storage
