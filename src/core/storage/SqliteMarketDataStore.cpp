#include "core/storage/SqliteMarketDataStore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimeZone>
#include <QUuid>

#include <algorithm>
#include <limits>

namespace oqt::storage {
namespace {

void setError(QString* target, const QString& message) {
    if (target) {
        *target = message;
    }
}

QString sqlError(const QSqlQuery& query) {
    return query.lastError().text();
}

QString utcIso(const QDateTime& value) {
    return value.toUTC().toString(Qt::ISODateWithMs);
}

} // namespace

SqliteMarketDataStore::SqliteMarketDataStore(QString databasePath)
    : m_databasePath(std::move(databasePath)),
      m_connectionName(QStringLiteral("oqt-market-store-%1")
                           .arg(QUuid::createUuid().toString(QUuid::WithoutBraces))) {
}

SqliteMarketDataStore::~SqliteMarketDataStore() {
    if (m_database.isValid()) {
        m_database.close();
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool SqliteMarketDataStore::open(QString* errorMessage) {
    if (m_database.isValid() && m_database.isOpen()) {
        return true;
    }
    if (m_databasePath.trimmed().isEmpty()) {
        setError(errorMessage, QStringLiteral("database path is required"));
        return false;
    }

    // A previous failed open may leave a registered but unusable connection.
    // Remove it before reusing this store instance.
    if (m_database.isValid()) {
        m_database.close();
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }

    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(m_databasePath);
    if (!m_database.open()) {
        const QString message = m_database.lastError().text();
        setError(errorMessage, message);
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
        return false;
    }

    // Keep SQLite contention bounded and allow persistent databases to serve
    // readers while a writer commits new market data.
    QSqlQuery busyTimeout(m_database);
    if (!busyTimeout.exec(QStringLiteral("PRAGMA busy_timeout = 5000"))) {
        setError(errorMessage, sqlError(busyTimeout));
        m_database.close();
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
        return false;
    }
    busyTimeout.finish();

    // SQLite cannot use WAL for an in-memory database; its native memory
    // journal is already appropriate for isolated unit tests.
    if (m_databasePath != QStringLiteral(":memory:")) {
        QSqlQuery journalMode(m_database);
        if (!journalMode.exec(QStringLiteral("PRAGMA journal_mode = WAL"))
            || !journalMode.next()
            || journalMode.value(0).toString().compare(QStringLiteral("wal"),
                                                        Qt::CaseInsensitive) != 0) {
            const QString message = journalMode.lastError().isValid()
                ? sqlError(journalMode)
                : QStringLiteral("SQLite could not enable WAL journal mode");
            setError(errorMessage, message);
            m_database.close();
            m_database = QSqlDatabase();
            QSqlDatabase::removeDatabase(m_connectionName);
            return false;
        }
        journalMode.finish();
    }

    if (!initializeSchema(errorMessage)) {
        m_database.close();
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
        return false;
    }
    return true;
}

bool SqliteMarketDataStore::isOpen() const {
    return m_database.isValid() && m_database.isOpen();
}

bool SqliteMarketDataStore::verifyIntegrity(QString* errorMessage) const {
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

bool SqliteMarketDataStore::backupTo(const QString& destinationPath,
                                        QString* errorMessage) const {
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
    if (QFile::exists(destination)) {
        setError(errorMessage, QStringLiteral("backup destination already exists"));
        return false;
    }

    // VACUUM INTO reads a consistent snapshot, including committed WAL data,
    // and writes a compact standalone database without copying live WAL files.
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

bool SqliteMarketDataStore::initializeSchema(QString* errorMessage) {
    constexpr int currentSchemaVersion = 1;

    QSqlQuery foreignKeys(m_database);
    if (!foreignKeys.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        setError(errorMessage, sqlError(foreignKeys));
        return false;
    }
    foreignKeys.finish();

    QSqlQuery versionQuery(m_database);
    if (!versionQuery.exec(QStringLiteral("PRAGMA user_version")) || !versionQuery.next()) {
        setError(errorMessage, sqlError(versionQuery));
        return false;
    }
    const int schemaVersion = versionQuery.value(0).toInt();
    versionQuery.finish();
    if (schemaVersion > currentSchemaVersion) {
        setError(errorMessage,
                 QStringLiteral("database schema version %1 is newer than supported version %2")
                     .arg(schemaVersion).arg(currentSchemaVersion));
        return false;
    }
    if (schemaVersion == currentSchemaVersion) {
        return true;
    }

    // Version 0 is the original unversioned schema. CREATE IF NOT EXISTS makes
    // this migration safe for databases created by the pre-versioning release.
    if (!m_database.transaction()) {
        setError(errorMessage, m_database.lastError().text());
        return false;
    }

    const QStringList statements = {
        QStringLiteral("CREATE TABLE IF NOT EXISTS market_series ("
                       "provider_id TEXT NOT NULL,"
                       "instrument_id TEXT NOT NULL,"
                       "interval TEXT NOT NULL,"
                       "provider_symbol TEXT NOT NULL,"
                       "currency TEXT NULL,"
                       "source_url TEXT NULL,"
                       "attribution_id TEXT NULL,"
                       "retrieved_at TEXT NULL,"
                       "quality_status INTEGER NOT NULL,"
                       "is_partial INTEGER NOT NULL,"
                       "is_truncated INTEGER NOT NULL,"
                       "missing_value_counts TEXT NOT NULL,"
                       "warnings TEXT NOT NULL,"
                       "next_page_cursor TEXT NULL,"
                       "PRIMARY KEY(provider_id, instrument_id, interval))"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS market_bars ("
                       "provider_id TEXT NOT NULL,"
                       "instrument_id TEXT NOT NULL,"
                       "interval TEXT NOT NULL,"
                       "timestamp_ms INTEGER NOT NULL,"
                       "open REAL NOT NULL,"
                       "high REAL NOT NULL,"
                       "low REAL NOT NULL,"
                       "close REAL NOT NULL,"
                       "adjusted_close REAL NULL,"
                       "volume REAL NULL,"
                       "PRIMARY KEY(provider_id, instrument_id, interval, timestamp_ms),"
                       "FOREIGN KEY(provider_id, instrument_id, interval) "
                       "REFERENCES market_series(provider_id, instrument_id, interval) "
                       "ON DELETE CASCADE)")
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

bool SqliteMarketDataStore::saveSeries(
    const oqt::data::CanonicalMarketSeries& series, QString* errorMessage) {
    if (!isOpen()) {
        setError(errorMessage, QStringLiteral("database is not open"));
        return false;
    }
    const QStringList validationErrors = series.validate();
    if (!validationErrors.isEmpty()) {
        setError(errorMessage, validationErrors.join(QStringLiteral("; ")));
        return false;
    }
    if (!m_database.transaction()) {
        setError(errorMessage, m_database.lastError().text());
        return false;
    }

    QSqlQuery header(m_database);
    header.prepare(QStringLiteral(
        "INSERT INTO market_series (provider_id,instrument_id,interval,provider_symbol,"
        "currency,source_url,attribution_id,retrieved_at,quality_status,is_partial,"
        "is_truncated,missing_value_counts,warnings,next_page_cursor) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?) "
        "ON CONFLICT(provider_id,instrument_id,interval) DO UPDATE SET "
        "provider_symbol=excluded.provider_symbol,currency=excluded.currency,"
        "source_url=excluded.source_url,attribution_id=excluded.attribution_id,"
        "retrieved_at=excluded.retrieved_at,quality_status=excluded.quality_status,"
        "is_partial=excluded.is_partial,is_truncated=excluded.is_truncated,"
        "missing_value_counts=excluded.missing_value_counts,warnings=excluded.warnings,"
        "next_page_cursor=excluded.next_page_cursor"));
    header.addBindValue(series.providerId);
    header.addBindValue(series.instrumentId);
    header.addBindValue(series.interval);
    header.addBindValue(series.providerSymbol);
    header.addBindValue(series.currency);
    header.addBindValue(series.provenance.sourceUrl);
    header.addBindValue(series.provenance.attributionId);
    header.addBindValue(utcIso(series.provenance.retrievedAt));
    header.addBindValue(static_cast<int>(series.quality.status));
    header.addBindValue(series.quality.isPartial ? 1 : 0);
    header.addBindValue(series.quality.isTruncated ? 1 : 0);

    QJsonObject missingCounts;
    for (auto it = series.quality.missingValueCounts.cbegin();
         it != series.quality.missingValueCounts.cend(); ++it) {
        missingCounts.insert(it.key(), static_cast<double>(it.value()));
    }
    header.addBindValue(QString::fromUtf8(
        QJsonDocument(missingCounts).toJson(QJsonDocument::Compact)));
    QJsonArray warnings;
    for (const QString& warning : series.quality.warnings) warnings.append(warning);
    header.addBindValue(QString::fromUtf8(
        QJsonDocument(warnings).toJson(QJsonDocument::Compact)));
    header.addBindValue(series.quality.nextPageCursor);

    if (!header.exec()) {
        setError(errorMessage, sqlError(header));
        m_database.rollback();
        return false;
    }

    QSqlQuery removeBars(m_database);
    removeBars.prepare(QStringLiteral(
        "DELETE FROM market_bars WHERE provider_id=? AND instrument_id=? AND interval=?"));
    removeBars.addBindValue(series.providerId);
    removeBars.addBindValue(series.instrumentId);
    removeBars.addBindValue(series.interval);
    if (!removeBars.exec()) {
        setError(errorMessage, sqlError(removeBars));
        m_database.rollback();
        return false;
    }

    // Insert bounded multi-row statements to reduce SQLite statement
    // executions without exceeding conservative host-parameter limits.
    constexpr qsizetype insertBatchSize = 50;
    for (qsizetype batchStart = 0; batchStart < series.bars.size();
         batchStart += insertBatchSize) {
        const qsizetype batchCount = qMin(insertBatchSize,
                                          series.bars.size() - batchStart);
        QStringList rowPlaceholders;
        rowPlaceholders.reserve(batchCount);
        for (qsizetype row = 0; row < batchCount; ++row) {
            rowPlaceholders.append(QStringLiteral("(?,?,?,?,?,?,?,?,?,?)"));
        }

        QSqlQuery insertBatch(m_database);
        insertBatch.prepare(QStringLiteral(
            "INSERT INTO market_bars (provider_id,instrument_id,interval,timestamp_ms,"
            "open,high,low,close,adjusted_close,volume) VALUES %1")
                                .arg(rowPlaceholders.join(QLatin1Char(','))));

        for (qsizetype row = 0; row < batchCount; ++row) {
            const auto& bar = series.bars.at(batchStart + row);
            insertBatch.addBindValue(series.providerId);
            insertBatch.addBindValue(series.instrumentId);
            insertBatch.addBindValue(series.interval);
            insertBatch.addBindValue(bar.timestamp.toUTC().toMSecsSinceEpoch());
            insertBatch.addBindValue(bar.open);
            insertBatch.addBindValue(bar.high);
            insertBatch.addBindValue(bar.low);
            insertBatch.addBindValue(bar.close);
            insertBatch.addBindValue(bar.adjustedClose.has_value()
                                         ? QVariant(*bar.adjustedClose) : QVariant());
            insertBatch.addBindValue(bar.volume.has_value()
                                         ? QVariant(*bar.volume) : QVariant());
        }

        if (!insertBatch.exec()) {
            setError(errorMessage, sqlError(insertBatch));
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


bool SqliteMarketDataStore::upsertSeries(
    const oqt::data::CanonicalMarketSeries& series, QString* errorMessage) {
    if (!isOpen()) {
        setError(errorMessage, QStringLiteral("database is not open"));
        return false;
    }
    const QStringList validationErrors = series.validate();
    if (!validationErrors.isEmpty()) {
        setError(errorMessage, validationErrors.join(QStringLiteral("; ")));
        return false;
    }
    if (!m_database.transaction()) {
        setError(errorMessage, m_database.lastError().text());
        return false;
    }

    QSqlQuery header(m_database);
    header.prepare(QStringLiteral(
        "INSERT INTO market_series (provider_id,instrument_id,interval,provider_symbol,"
        "currency,source_url,attribution_id,retrieved_at,quality_status,is_partial,"
        "is_truncated,missing_value_counts,warnings,next_page_cursor) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?) "
        "ON CONFLICT(provider_id,instrument_id,interval) DO UPDATE SET "
        "provider_symbol=excluded.provider_symbol,currency=excluded.currency,"
        "source_url=excluded.source_url,attribution_id=excluded.attribution_id,"
        "retrieved_at=excluded.retrieved_at,quality_status=excluded.quality_status,"
        "is_partial=excluded.is_partial,is_truncated=excluded.is_truncated,"
        "missing_value_counts=excluded.missing_value_counts,warnings=excluded.warnings,"
        "next_page_cursor=excluded.next_page_cursor"));
    header.addBindValue(series.providerId);
    header.addBindValue(series.instrumentId);
    header.addBindValue(series.interval);
    header.addBindValue(series.providerSymbol);
    header.addBindValue(series.currency);
    header.addBindValue(series.provenance.sourceUrl);
    header.addBindValue(series.provenance.attributionId);
    header.addBindValue(utcIso(series.provenance.retrievedAt));
    header.addBindValue(static_cast<int>(series.quality.status));
    header.addBindValue(series.quality.isPartial ? 1 : 0);
    header.addBindValue(series.quality.isTruncated ? 1 : 0);

    QJsonObject missingCounts;
    for (auto it = series.quality.missingValueCounts.cbegin();
         it != series.quality.missingValueCounts.cend(); ++it) {
        missingCounts.insert(it.key(), static_cast<double>(it.value()));
    }
    header.addBindValue(QString::fromUtf8(
        QJsonDocument(missingCounts).toJson(QJsonDocument::Compact)));
    QJsonArray warnings;
    for (const QString& warning : series.quality.warnings) warnings.append(warning);
    header.addBindValue(QString::fromUtf8(
        QJsonDocument(warnings).toJson(QJsonDocument::Compact)));
    header.addBindValue(series.quality.nextPageCursor);

    if (!header.exec()) {
        setError(errorMessage, sqlError(header));
        m_database.rollback();
        return false;
    }


    // Insert bounded multi-row statements to reduce SQLite statement
    // executions without exceeding conservative host-parameter limits.
    constexpr qsizetype insertBatchSize = 50;
    for (qsizetype batchStart = 0; batchStart < series.bars.size();
         batchStart += insertBatchSize) {
        const qsizetype batchCount = qMin(insertBatchSize,
                                          series.bars.size() - batchStart);
        QStringList rowPlaceholders;
        rowPlaceholders.reserve(batchCount);
        for (qsizetype row = 0; row < batchCount; ++row) {
            rowPlaceholders.append(QStringLiteral("(?,?,?,?,?,?,?,?,?,?)"));
        }

        QSqlQuery insertBatch(m_database);
        insertBatch.prepare(QStringLiteral(
            "INSERT INTO market_bars (provider_id,instrument_id,interval,timestamp_ms,"
            "open,high,low,close,adjusted_close,volume) VALUES %1 "
            "ON CONFLICT(provider_id,instrument_id,interval,timestamp_ms) DO UPDATE SET "
            "open=excluded.open,high=excluded.high,low=excluded.low,close=excluded.close,"
            "adjusted_close=excluded.adjusted_close,volume=excluded.volume")
                                .arg(rowPlaceholders.join(QLatin1Char(','))));

        for (qsizetype row = 0; row < batchCount; ++row) {
            const auto& bar = series.bars.at(batchStart + row);
            insertBatch.addBindValue(series.providerId);
            insertBatch.addBindValue(series.instrumentId);
            insertBatch.addBindValue(series.interval);
            insertBatch.addBindValue(bar.timestamp.toUTC().toMSecsSinceEpoch());
            insertBatch.addBindValue(bar.open);
            insertBatch.addBindValue(bar.high);
            insertBatch.addBindValue(bar.low);
            insertBatch.addBindValue(bar.close);
            insertBatch.addBindValue(bar.adjustedClose.has_value()
                                         ? QVariant(*bar.adjustedClose) : QVariant());
            insertBatch.addBindValue(bar.volume.has_value()
                                         ? QVariant(*bar.volume) : QVariant());
        }

        if (!insertBatch.exec()) {
            setError(errorMessage, sqlError(insertBatch));
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

std::optional<oqt::data::CanonicalMarketSeries> SqliteMarketDataStore::loadSeries(
    const QString& providerId, const QString& instrumentId, const QString& interval,
    QString* errorMessage) const {
    return loadSeriesImpl(providerId, instrumentId, interval, std::nullopt, std::nullopt,
                          0, errorMessage);
}

std::optional<CanonicalMarketSeriesPage> SqliteMarketDataStore::loadSeriesPage(
    const QString& providerId, const QString& instrumentId, const QString& interval,
    qsizetype pageSize, qint64 offset, QString* errorMessage) const {
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
    auto series = loadSeriesImpl(providerId, instrumentId, interval, std::nullopt,
                                 std::nullopt, pageSize, errorMessage, offset, &hasMore);
    if (!series) return std::nullopt;
    return CanonicalMarketSeriesPage{std::move(*series), hasMore, offset, pageSize};
}

std::optional<oqt::data::CanonicalMarketSeries> SqliteMarketDataStore::loadSeriesRange(
    const QString& providerId, const QString& instrumentId, const QString& interval,
    const QDateTime& startInclusive, const QDateTime& endExclusive,
    QString* errorMessage, qsizetype maxBars) const {
    if (!startInclusive.isValid() || !endExclusive.isValid()
        || startInclusive.timeSpec() != Qt::UTC || endExclusive.timeSpec() != Qt::UTC) {
        setError(errorMessage, QStringLiteral("range bounds must be valid UTC timestamps"));
        return std::nullopt;
    }
    const qint64 startMs = startInclusive.toMSecsSinceEpoch();
    const qint64 endMs = endExclusive.toMSecsSinceEpoch();
    if (startMs >= endMs) {
        setError(errorMessage, QStringLiteral("range start must be earlier than range end"));
        return std::nullopt;
    }
    if (maxBars < 0) {
        setError(errorMessage, QStringLiteral("maxBars cannot be negative"));
        return std::nullopt;
    }
    if (maxBars == std::numeric_limits<qsizetype>::max()) {
        setError(errorMessage, QStringLiteral("maxBars is too large"));
        return std::nullopt;
    }
    return loadSeriesImpl(providerId, instrumentId, interval, startMs, endMs,
                          maxBars, errorMessage);
}

std::optional<oqt::data::CanonicalMarketSeries> SqliteMarketDataStore::loadSeriesImpl(
    const QString& providerId, const QString& instrumentId, const QString& interval,
    std::optional<qint64> startInclusiveMs, std::optional<qint64> endExclusiveMs,
    qsizetype maxBars, QString* errorMessage, qint64 offset, bool* hasMore) const {
    if (hasMore) *hasMore = false;
    if (!isOpen()) {
        setError(errorMessage, QStringLiteral("database is not open"));
        return std::nullopt;
    }

    QSqlQuery header(m_database);
    header.prepare(QStringLiteral(
        "SELECT provider_symbol,currency,source_url,attribution_id,retrieved_at,"
        "quality_status,is_partial,is_truncated,missing_value_counts,warnings,next_page_cursor "
        "FROM market_series WHERE provider_id=? AND instrument_id=? AND interval=?"));
    header.addBindValue(providerId);
    header.addBindValue(instrumentId);
    header.addBindValue(interval);
    if (!header.exec()) {
        setError(errorMessage, sqlError(header));
        return std::nullopt;
    }
    if (!header.next()) return std::nullopt;

    oqt::data::CanonicalMarketSeries series;
    series.providerId = providerId;
    series.instrumentId = instrumentId;
    series.interval = interval;
    series.providerSymbol = header.value(0).toString();
    series.currency = header.value(1).toString();
    series.provenance.sourceUrl = header.value(2).toString();
    series.provenance.attributionId = header.value(3).toString();
    series.provenance.retrievedAt = QDateTime::fromString(header.value(4).toString(),
                                                           Qt::ISODateWithMs);
    series.quality.status = static_cast<oqt::data::DataStatus>(header.value(5).toInt());
    series.quality.isPartial = header.value(6).toBool();
    series.quality.isTruncated = header.value(7).toBool();
    const QJsonDocument missingDoc = QJsonDocument::fromJson(header.value(8).toString().toUtf8());
    const QJsonObject missingCounts = missingDoc.object();
    for (auto it = missingCounts.begin(); it != missingCounts.end(); ++it) {
        series.quality.missingValueCounts.insert(it.key(), static_cast<qsizetype>(it.value().toDouble()));
    }
    const QJsonDocument warningsDoc = QJsonDocument::fromJson(header.value(9).toString().toUtf8());
    for (const QJsonValue& warning : warningsDoc.array()) {
        if (warning.isString()) series.quality.warnings.append(warning.toString());
    }
    series.quality.nextPageCursor = header.value(10).toString();

    QSqlQuery bars(m_database);
    QString barsSql = QStringLiteral(
        "SELECT timestamp_ms,open,high,low,close,adjusted_close,volume FROM market_bars "
        "WHERE provider_id=? AND instrument_id=? AND interval=?");
    if (startInclusiveMs.has_value()) {
        barsSql += QStringLiteral(" AND timestamp_ms>=?");
    }
    if (endExclusiveMs.has_value()) {
        barsSql += QStringLiteral(" AND timestamp_ms<?");
    }
    // Page queries select the newest slice first. Range reads preserve their
    // established earliest-first semantics, including bounded maxBars reads.
    const bool newestFirstPage = hasMore != nullptr;
    barsSql += newestFirstPage
        ? QStringLiteral(" ORDER BY timestamp_ms DESC")
        : QStringLiteral(" ORDER BY timestamp_ms ASC");
    if (maxBars > 0) {
        barsSql += QStringLiteral(" LIMIT ? OFFSET ?");
    }
    bars.prepare(barsSql);
    bars.addBindValue(providerId);
    bars.addBindValue(instrumentId);
    bars.addBindValue(interval);
    if (startInclusiveMs.has_value()) bars.addBindValue(*startInclusiveMs);
    if (endExclusiveMs.has_value()) bars.addBindValue(*endExclusiveMs);
    if (maxBars > 0) {
        bars.addBindValue(maxBars + 1);
        bars.addBindValue(offset);
    }
    if (!bars.exec()) {
        setError(errorMessage, sqlError(bars));
        return std::nullopt;
    }
    while (bars.next()) {
        oqt::data::CanonicalMarketBar bar;
        bar.timestamp = QDateTime::fromMSecsSinceEpoch(bars.value(0).toLongLong(), QTimeZone::UTC);
        bar.open = bars.value(1).toDouble();
        bar.high = bars.value(2).toDouble();
        bar.low = bars.value(3).toDouble();
        bar.close = bars.value(4).toDouble();
        if (!bars.isNull(5)) bar.adjustedClose = bars.value(5).toDouble();
        if (!bars.isNull(6)) bar.volume = bars.value(6).toDouble();
        series.bars.append(std::move(bar));
    }

    // Read one extra row so truncation is reported only when more matching
    // candles actually exist beyond the requested cap.
    if (maxBars > 0 && series.bars.size() > maxBars) {
        if (hasMore) *hasMore = true;
        series.bars.removeLast();
        series.quality.status = oqt::data::DataStatus::Partial;
        series.quality.isPartial = true;
        series.quality.isTruncated = true;
        series.quality.warnings.append(
            QStringLiteral("OHLCV range result truncated at maxBars=%1").arg(maxBars));
    }

    // Only page queries are fetched newest-first; reverse those slices so
    // each page remains chronological. Range reads are already ascending.
    if (newestFirstPage) {
        std::reverse(series.bars.begin(), series.bars.end());
    }

    // A valid query can return no candles even when the stored full series
    // was non-empty. Represent the returned slice as an empty result.
    if (series.bars.isEmpty()) {
        series.quality.status = oqt::data::DataStatus::Empty;
        series.quality.isPartial = false;
    }

    const QStringList validationErrors = series.validate();
    if (!validationErrors.isEmpty()) {
        setError(errorMessage, validationErrors.join(QStringLiteral("; ")));
        return std::nullopt;
    }
    return series;
}

} // namespace oqt::storage
