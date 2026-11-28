#include "core/storage/SqliteMarketDataStore.h"

#include <QtTest>

#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimeZone>

using namespace oqt::data;
using oqt::storage::SqliteMarketDataStore;

class SqliteMarketDataStoreTest final : public QObject {
    Q_OBJECT

private:
    static CanonicalMarketSeries sampleSeries() {
        CanonicalMarketSeries series;
        series.providerId = QStringLiteral("yahoo_finance");
        series.instrumentId = QStringLiteral("equity.aapl");
        series.providerSymbol = QStringLiteral("AAPL");
        series.interval = QStringLiteral("1d");
        series.currency = QStringLiteral("USD");
        series.provenance.sourceUrl =
            QStringLiteral("https://query1.finance.yahoo.com/v8/finance/chart/");
        series.provenance.attributionId = QStringLiteral("Yahoo Finance");
        series.provenance.retrievedAt =
            QDateTime::fromString(QStringLiteral("2026-11-28T00:00:00.000Z"),
                                  Qt::ISODateWithMs);
        series.quality.status = DataStatus::Partial;
        series.quality.isPartial = true;
        series.quality.missingValueCounts.insert(QStringLiteral("volume"), 1);
        series.quality.warnings.append(QStringLiteral("volume unavailable"));
        series.quality.nextPageCursor = QStringLiteral("cursor-1");

        CanonicalMarketBar first;
        first.timestamp = QDateTime::fromString(
            QStringLiteral("2026-11-26T00:00:00.000Z"), Qt::ISODateWithMs);
        first.open = 100.0;
        first.high = 105.0;
        first.low = 99.0;
        first.close = 103.0;
        first.adjustedClose = 102.5;
        first.volume = 1200.0;
        series.bars.append(first);

        CanonicalMarketBar second;
        second.timestamp = QDateTime::fromString(
            QStringLiteral("2026-11-27T00:00:00.000Z"), Qt::ISODateWithMs);
        second.open = 103.0;
        second.high = 107.0;
        second.low = 101.0;
        second.close = 106.0;
        // Optional fields intentionally absent; SQL NULL must survive round-trip.
        series.bars.append(second);
        return series;
    }

private slots:
    void opensAndCreatesSchema() {
        SqliteMarketDataStore store(QStringLiteral(":memory:"));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));
        QVERIFY(store.isOpen());
    }

    void verifiesHealthyDatabaseIntegrity() {
        SqliteMarketDataStore store(QStringLiteral(":memory:"));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));
        QVERIFY2(store.saveSeries(sampleSeries(), &error), qPrintable(error));
        QVERIFY2(store.verifyIntegrity(&error), qPrintable(error));
        QVERIFY(error.isEmpty());
    }

    void createsConsistentStandaloneBackup() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString databasePath = directory.filePath(QStringLiteral("source.sqlite"));
        const QString backupPath = directory.filePath(QStringLiteral("backup.sqlite"));

        SqliteMarketDataStore sourceStore(databasePath);
        QString error;
        QVERIFY2(sourceStore.open(&error), qPrintable(error));
        const CanonicalMarketSeries source = sampleSeries();
        QVERIFY2(sourceStore.saveSeries(source, &error), qPrintable(error));

        QVERIFY2(sourceStore.backupTo(backupPath, &error), qPrintable(error));
        QVERIFY(QFile::exists(backupPath));

        error.clear();
        QVERIFY(!sourceStore.backupTo(databasePath, &error));
        QVERIFY(error.contains(QStringLiteral("must differ")));
        error.clear();
        QVERIFY(!sourceStore.backupTo(backupPath, &error));
        QVERIFY(error.contains(QStringLiteral("already exists")));

        SqliteMarketDataStore backupStore(backupPath);
        error.clear();
        QVERIFY2(backupStore.open(&error), qPrintable(error));
        QVERIFY2(backupStore.verifyIntegrity(&error), qPrintable(error));
        const auto restored = backupStore.loadSeries(source.providerId, source.instrumentId,
                                                     source.interval, &error);
        QVERIFY2(restored.has_value(), qPrintable(error));
        QCOMPARE(restored->bars.size(), source.bars.size());
        QCOMPARE(restored->bars.first().timestamp, source.bars.first().timestamp);
        QCOMPARE(restored->bars.last().close, source.bars.last().close);
        QCOMPARE(restored->provenance.attributionId, source.provenance.attributionId);
        QVERIFY(restored->validate().isEmpty());
    }

    void rejectsBackupWhenDatabaseIsClosed() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SqliteMarketDataStore store(directory.filePath(QStringLiteral("closed.sqlite")));
        QString error;
        QVERIFY(!store.backupTo(directory.filePath(QStringLiteral("backup.sqlite")), &error));
        QVERIFY(error.contains(QStringLiteral("not open")));
    }

    void detectsForeignKeyViolationsDuringIntegrityVerification() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString databasePath = directory.filePath(QStringLiteral("foreign-key-check.sqlite"));

        {
            SqliteMarketDataStore store(databasePath);
            QString error;
            QVERIFY2(store.open(&error), qPrintable(error));
        }

        const QString connectionName = QStringLiteral("foreign-key-violation-seed");
        {
            QSqlDatabase database = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), connectionName);
            database.setDatabaseName(databasePath);
            QVERIFY(database.open());
            QSqlQuery query(database);
            QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys = OFF")));
            QVERIFY2(query.exec(QStringLiteral(
                "INSERT INTO market_bars (provider_id,instrument_id,interval,timestamp_ms,"
                "open,high,low,close,adjusted_close,volume) "
                "VALUES ('orphan','equity.orphan','1d',1795737600000,10,12,9,11,NULL,NULL)")),
                qPrintable(query.lastError().text()));
            database.close();
        }
        QSqlDatabase::removeDatabase(connectionName);

        SqliteMarketDataStore store(databasePath);
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));
        QVERIFY(!store.verifyIntegrity(&error));
        QVERIFY(error.contains(QStringLiteral("foreign-key violation")));
        QVERIFY(error.contains(QStringLiteral("market_bars")));
    }

    void upsertsIncrementalBarsWithoutDeletingHistory() {
        SqliteMarketDataStore store(QStringLiteral(":memory:"));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));

        const CanonicalMarketSeries original = sampleSeries();
        QVERIFY2(store.saveSeries(original, &error), qPrintable(error));

        CanonicalMarketSeries update = original;
        update.provenance.retrievedAt = QDateTime::fromString(
            QStringLiteral("2026-11-29T00:00:00.000Z"), Qt::ISODateWithMs);
        update.quality.status = DataStatus::Ok;
        update.quality.isPartial = false;
        update.quality.missingValueCounts.clear();
        update.quality.warnings.clear();
        update.bars.clear();

        CanonicalMarketBar corrected;
        corrected.timestamp = original.bars.at(1).timestamp;
        corrected.open = 103.0;
        corrected.high = 109.0;
        corrected.low = 102.0;
        corrected.close = 108.0;
        corrected.adjustedClose = 107.5;
        corrected.volume = 1500.0;
        update.bars.append(corrected);

        CanonicalMarketBar newest;
        newest.timestamp = QDateTime::fromString(
            QStringLiteral("2026-11-28T00:00:00.000Z"), Qt::ISODateWithMs);
        newest.open = 108.0;
        newest.high = 111.0;
        newest.low = 107.0;
        newest.close = 110.0;
        newest.volume = 1800.0;
        update.bars.append(newest);

        QVERIFY(update.validate().isEmpty());
        QVERIFY2(store.upsertSeries(update, &error), qPrintable(error));

        const auto merged = store.loadSeries(original.providerId, original.instrumentId,
                                             original.interval, &error);
        QVERIFY2(merged.has_value(), qPrintable(error));
        QCOMPARE(merged->bars.size(), qsizetype(3));
        QCOMPARE(merged->bars.at(0).timestamp, original.bars.at(0).timestamp);
        QCOMPARE(merged->bars.at(0).close, original.bars.at(0).close);
        QCOMPARE(merged->bars.at(1).timestamp, corrected.timestamp);
        QCOMPARE(merged->bars.at(1).close, 108.0);
        QCOMPARE(merged->bars.at(1).volume, std::optional<double>(1500.0));
        QCOMPARE(merged->bars.at(2).timestamp, newest.timestamp);
        QCOMPARE(merged->bars.at(2).close, 110.0);
        QVERIFY(!merged->bars.at(2).adjustedClose.has_value());
        QCOMPARE(merged->provenance.retrievedAt, update.provenance.retrievedAt);
        QCOMPARE(merged->quality.status, DataStatus::Ok);
        QVERIFY(merged->validate().isEmpty());
    }

    void rejectsInvalidIncrementalSeriesWithoutWriting() {
        SqliteMarketDataStore store(QStringLiteral(":memory:"));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));
        CanonicalMarketSeries invalid;
        QVERIFY(!store.upsertSeries(invalid, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!store.loadSeries(QStringLiteral("missing"), QStringLiteral("missing"),
                                  QStringLiteral("1d"), nullptr).has_value());
    }

    void pagesMarketBarsInStableDatabaseOrder() {
        SqliteMarketDataStore store(QStringLiteral(":memory:"));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));
        const CanonicalMarketSeries source = sampleSeries();
        QVERIFY2(store.saveSeries(source, &error), qPrintable(error));

        const auto first = store.loadSeriesPage(source.providerId, source.instrumentId,
                                                source.interval, 1, 0, &error);
        QVERIFY2(first.has_value(), qPrintable(error));
        QCOMPARE(first->series.bars.size(), qsizetype(1));
        QCOMPARE(first->series.bars.first().timestamp, source.bars.first().timestamp);
        QVERIFY(first->hasMore);
        QCOMPARE(first->offset, qint64(0));

        const auto second = store.loadSeriesPage(source.providerId, source.instrumentId,
                                                 source.interval, 1, 1, &error);
        QVERIFY2(second.has_value(), qPrintable(error));
        QCOMPARE(second->series.bars.size(), qsizetype(1));
        QCOMPARE(second->series.bars.first().timestamp, source.bars.last().timestamp);
        QVERIFY(!second->hasMore);

        const auto empty = store.loadSeriesPage(source.providerId, source.instrumentId,
                                                source.interval, 1, 2, &error);
        QVERIFY2(empty.has_value(), qPrintable(error));
        QVERIFY(empty->series.bars.isEmpty());
        QVERIFY(!empty->hasMore);

        error.clear();
        QVERIFY(!store.loadSeriesPage(source.providerId, source.instrumentId,
                                      source.interval, 0, 0, &error));
        QVERIFY(error.contains(QStringLiteral("pageSize")));
        error.clear();
        QVERIFY(!store.loadSeriesPage(source.providerId, source.instrumentId,
                                      source.interval, 1, -1, &error));
        QVERIFY(error.contains(QStringLiteral("offset")));
    }

    void roundTripsBarsAndProvenance() {
        SqliteMarketDataStore store(QStringLiteral(":memory:"));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));

        const CanonicalMarketSeries source = sampleSeries();
        QVERIFY2(store.saveSeries(source, &error), qPrintable(error));

        const auto loaded = store.loadSeries(source.providerId, source.instrumentId,
                                             source.interval, &error);
        QVERIFY2(loaded.has_value(), qPrintable(error));
        QCOMPARE(loaded->providerId, source.providerId);
        QCOMPARE(loaded->instrumentId, source.instrumentId);
        QCOMPARE(loaded->providerSymbol, source.providerSymbol);
        QCOMPARE(loaded->interval, source.interval);
        QCOMPARE(loaded->currency, source.currency);
        QCOMPARE(loaded->bars.size(), qsizetype(2));
        QCOMPARE(loaded->bars.at(0).timestamp, source.bars.at(0).timestamp);
        QCOMPARE(loaded->bars.at(0).close, 103.0);
        QCOMPARE(*loaded->bars.at(0).adjustedClose, 102.5);
        QCOMPARE(*loaded->bars.at(0).volume, 1200.0);
        QVERIFY(!loaded->bars.at(1).adjustedClose.has_value());
        QVERIFY(!loaded->bars.at(1).volume.has_value());
        QCOMPARE(loaded->provenance.sourceUrl, source.provenance.sourceUrl);
        QCOMPARE(loaded->provenance.attributionId, source.provenance.attributionId);
        QCOMPARE(loaded->provenance.retrievedAt, source.provenance.retrievedAt);
        QCOMPARE(loaded->quality.status, DataStatus::Partial);
        QVERIFY(loaded->quality.isPartial);
        QCOMPARE(loaded->quality.missingValueCounts.value(QStringLiteral("volume")), qsizetype(1));
        QCOMPARE(loaded->quality.warnings, source.quality.warnings);
        QCOMPARE(loaded->quality.nextPageCursor, source.quality.nextPageCursor);
        QVERIFY(loaded->validate().isEmpty());
    }

    void loadsBarsWithinHalfOpenUtcRange() {
        SqliteMarketDataStore store(QStringLiteral(":memory:"));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));
        const CanonicalMarketSeries source = sampleSeries();
        QVERIFY2(store.saveSeries(source, &error), qPrintable(error));

        const QDateTime start = QDateTime::fromString(
            QStringLiteral("2026-11-26T00:00:00.000Z"), Qt::ISODateWithMs);
        const QDateTime middle = QDateTime::fromString(
            QStringLiteral("2026-11-27T00:00:00.000Z"), Qt::ISODateWithMs);
        const QDateTime end = QDateTime::fromString(
            QStringLiteral("2026-11-28T00:00:00.000Z"), Qt::ISODateWithMs);

        const auto firstDay = store.loadSeriesRange(
            source.providerId, source.instrumentId, source.interval, start, middle, &error);
        QVERIFY2(firstDay.has_value(), qPrintable(error));
        QCOMPARE(firstDay->bars.size(), qsizetype(1));
        QCOMPARE(firstDay->bars.first().timestamp, start);
        QCOMPARE(firstDay->bars.first().close, 103.0);
        QCOMPARE(firstDay->providerSymbol, source.providerSymbol);
        QCOMPARE(firstDay->provenance.attributionId, source.provenance.attributionId);
        QVERIFY(firstDay->validate().isEmpty());

        const auto secondDay = store.loadSeriesRange(
            source.providerId, source.instrumentId, source.interval, middle, end, &error);
        QVERIFY2(secondDay.has_value(), qPrintable(error));
        QCOMPARE(secondDay->bars.size(), qsizetype(1));
        QCOMPARE(secondDay->bars.first().timestamp, middle);
        QCOMPARE(secondDay->bars.first().close, 106.0);
        QVERIFY(!secondDay->bars.first().volume.has_value());
        QVERIFY(secondDay->validate().isEmpty());
    }

    void limitsOhlcvRangeResultsInAscendingOrder() {
        SqliteMarketDataStore store(QStringLiteral(":memory:"));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));
        const CanonicalMarketSeries source = sampleSeries();
        QVERIFY2(store.saveSeries(source, &error), qPrintable(error));

        const QDateTime start = source.bars.first().timestamp;
        const QDateTime end = source.bars.last().timestamp.addMSecs(1);

        const auto limited = store.loadSeriesRange(
            source.providerId, source.instrumentId, source.interval,
            start, end, &error, 1);
        QVERIFY2(limited.has_value(), qPrintable(error));
        QCOMPARE(limited->bars.size(), qsizetype(1));
        QCOMPARE(limited->bars.first().timestamp, source.bars.first().timestamp);
        QCOMPARE(limited->bars.first().close, source.bars.first().close);
        QCOMPARE(limited->quality.status, DataStatus::Partial);
        QVERIFY(limited->quality.isPartial);
        QVERIFY(limited->quality.isTruncated);
        QVERIFY(limited->quality.warnings.last().contains(QStringLiteral("maxBars=1")));
        QVERIFY(limited->validate().isEmpty());

        const auto exactLimit = store.loadSeriesRange(
            source.providerId, source.instrumentId, source.interval,
            start, end, &error, source.bars.size());
        QVERIFY2(exactLimit.has_value(), qPrintable(error));
        QCOMPARE(exactLimit->bars.size(), source.bars.size());
        QVERIFY(!exactLimit->quality.isTruncated);

        const auto unlimited = store.loadSeriesRange(
            source.providerId, source.instrumentId, source.interval,
            start, end, &error);
        QVERIFY2(unlimited.has_value(), qPrintable(error));
        QCOMPARE(unlimited->bars.size(), source.bars.size());
    }

    void rejectsNegativeOhlcvRangeLimit() {
        SqliteMarketDataStore store(QStringLiteral(":memory:"));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));
        const QDateTime start = QDateTime::fromString(
            QStringLiteral("2026-11-26T00:00:00.000Z"), Qt::ISODateWithMs);
        const QDateTime end = QDateTime::fromString(
            QStringLiteral("2026-11-28T00:00:00.000Z"), Qt::ISODateWithMs);

        const auto result = store.loadSeriesRange(
            QStringLiteral("yahoo_finance"), QStringLiteral("equity.aapl"),
            QStringLiteral("1d"), start, end, &error, -1);
        QVERIFY(!result.has_value());
        QVERIFY(error.contains(QStringLiteral("maxBars cannot be negative")));
    }

    void returnsValidEmptySeriesForRangeWithoutBars() {
        SqliteMarketDataStore store(QStringLiteral(":memory:"));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));
        const CanonicalMarketSeries source = sampleSeries();
        QVERIFY2(store.saveSeries(source, &error), qPrintable(error));

        const QDateTime start = QDateTime::fromString(
            QStringLiteral("2026-11-28T00:00:00.000Z"), Qt::ISODateWithMs);
        const QDateTime end = QDateTime::fromString(
            QStringLiteral("2026-11-29T00:00:00.000Z"), Qt::ISODateWithMs);
        const auto empty = store.loadSeriesRange(
            source.providerId, source.instrumentId, source.interval, start, end, &error);

        QVERIFY2(empty.has_value(), qPrintable(error));
        QVERIFY(empty->bars.isEmpty());
        QCOMPARE(empty->quality.status, DataStatus::Empty);
        QVERIFY(!empty->quality.isPartial);
        QVERIFY(empty->validate().isEmpty());
    }

    void rejectsInvalidOhlcvRangeBounds() {
        SqliteMarketDataStore store(QStringLiteral(":memory:"));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));

        const QDateTime start = QDateTime::fromString(
            QStringLiteral("2026-11-26T00:00:00.000Z"), Qt::ISODateWithMs);
        const QDateTime end = QDateTime::fromString(
            QStringLiteral("2026-11-28T00:00:00.000Z"), Qt::ISODateWithMs);
        const QDateTime localTime(QDate(2026, 11, 26), QTime(0, 0), QTimeZone::systemTimeZone());

        QVERIFY(!store.loadSeriesRange(QStringLiteral("yahoo_finance"),
                                       QStringLiteral("equity.aapl"), QStringLiteral("1d"),
                                       QDateTime(), end, &error).has_value());
        QVERIFY(error.contains(QStringLiteral("valid UTC")));
        error.clear();

        QVERIFY(!store.loadSeriesRange(QStringLiteral("yahoo_finance"),
                                       QStringLiteral("equity.aapl"), QStringLiteral("1d"),
                                       localTime, end, &error).has_value());
        QVERIFY(error.contains(QStringLiteral("valid UTC")));
        error.clear();

        QVERIFY(!store.loadSeriesRange(QStringLiteral("yahoo_finance"),
                                       QStringLiteral("equity.aapl"), QStringLiteral("1d"),
                                       end, start, &error).has_value());
        QVERIFY(error.contains(QStringLiteral("earlier")));
    }

    void replacingSeriesRemovesOldBarsAtomically() {
        SqliteMarketDataStore store(QStringLiteral(":memory:"));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));

        auto series = sampleSeries();
        QVERIFY2(store.saveSeries(series, &error), qPrintable(error));
        series.bars.removeFirst();
        series.bars[0].timestamp = QDateTime::fromString(
            QStringLiteral("2026-11-28T00:00:00.000Z"), Qt::ISODateWithMs);
        series.bars[0].close = 104.0;
        QVERIFY2(store.saveSeries(series, &error), qPrintable(error));

        const auto loaded = store.loadSeries(series.providerId, series.instrumentId,
                                             series.interval, &error);
        QVERIFY2(loaded.has_value(), qPrintable(error));
        QCOMPARE(loaded->bars.size(), qsizetype(1));
        QCOMPARE(loaded->bars.first().close, 104.0);
    }

    void persistsValidEmptySeries() {
        SqliteMarketDataStore store(QStringLiteral(":memory:"));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));

        CanonicalMarketSeries empty;
        empty.providerId = QStringLiteral("yahoo_finance");
        empty.instrumentId = QStringLiteral("equity.empty");
        empty.providerSymbol = QStringLiteral("EMPTY");
        empty.interval = QStringLiteral("1d");
        empty.quality.status = DataStatus::Empty;
        QVERIFY2(store.saveSeries(empty, &error), qPrintable(error));

        const auto loaded = store.loadSeries(empty.providerId, empty.instrumentId,
                                             empty.interval, &error);
        QVERIFY2(loaded.has_value(), qPrintable(error));
        QVERIFY(loaded->bars.isEmpty());
        QCOMPARE(loaded->quality.status, DataStatus::Empty);
    }


    void persistsAcrossStoreInstances() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString databasePath = directory.filePath(QStringLiteral("market-data.sqlite"));
        const CanonicalMarketSeries source = sampleSeries();

        {
            SqliteMarketDataStore writer(databasePath);
            QString error;
            QVERIFY2(writer.open(&error), qPrintable(error));
            QVERIFY2(writer.saveSeries(source, &error), qPrintable(error));
        }

        {
            SqliteMarketDataStore reader(databasePath);
            QString error;
            QVERIFY2(reader.open(&error), qPrintable(error));
            const auto loaded = reader.loadSeries(source.providerId, source.instrumentId,
                                                  source.interval, &error);
            QVERIFY2(loaded.has_value(), qPrintable(error));
            QCOMPARE(loaded->bars.size(), source.bars.size());
            QCOMPARE(loaded->bars.at(0).timestamp, source.bars.at(0).timestamp);
            QCOMPARE(loaded->bars.at(0).close, source.bars.at(0).close);
            QCOMPARE(loaded->bars.at(0).adjustedClose, source.bars.at(0).adjustedClose);
            QCOMPARE(loaded->bars.at(1).volume, source.bars.at(1).volume);
            QCOMPARE(loaded->provenance.sourceUrl, source.provenance.sourceUrl);
            QCOMPARE(loaded->provenance.attributionId, source.provenance.attributionId);
            QCOMPARE(loaded->provenance.retrievedAt, source.provenance.retrievedAt);
            QCOMPARE(loaded->quality.status, source.quality.status);
            QCOMPARE(loaded->quality.missingValueCounts, source.quality.missingValueCounts);
            QCOMPARE(loaded->quality.warnings, source.quality.warnings);
            QCOMPARE(loaded->quality.nextPageCursor, source.quality.nextPageCursor);
            QVERIFY(loaded->validate().isEmpty());
        }
    }

    void recoversAfterSchemaInitializationFailure() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString databasePath = directory.filePath(QStringLiteral("corrupt.sqlite"));
        {
            QFile corruptFile(databasePath);
            QVERIFY(corruptFile.open(QIODevice::WriteOnly));
            QVERIFY(corruptFile.write("not a sqlite database") > 0);
        }

        SqliteMarketDataStore store(databasePath);
        QString error;
        QVERIFY(!store.open(&error));
        QVERIFY(!store.isOpen());
        QVERIFY(!error.isEmpty());

        QVERIFY(QFile::remove(databasePath));
        error.clear();
        QVERIFY2(store.open(&error), qPrintable(error));
        QVERIFY(store.isOpen());
    }


    void initializesSchemaVersionForNewDatabase() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString databasePath = directory.filePath(QStringLiteral("versioned.sqlite"));
        {
            SqliteMarketDataStore store(databasePath);
            QString error;
            QVERIFY2(store.open(&error), qPrintable(error));
        }

        const QString connectionName = QStringLiteral("schema-version-check");
        {
            QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                               connectionName);
            database.setDatabaseName(databasePath);
            QVERIFY(database.open());
            QSqlQuery query(database);
            QVERIFY(query.exec(QStringLiteral("PRAGMA user_version")));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), 1);
            database.close();
        }
        QSqlDatabase::removeDatabase(connectionName);
    }

    void migratesUnversionedDatabaseWithoutLosingRows() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString databasePath = directory.filePath(QStringLiteral("legacy.sqlite"));
        const QString connectionName = QStringLiteral("legacy-schema-seed");
        {
            QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                               connectionName);
            database.setDatabaseName(databasePath);
            QVERIFY(database.open());
            QSqlQuery query(database);
            const QStringList schema = {
                QStringLiteral("CREATE TABLE market_series ("
                               "provider_id TEXT NOT NULL,instrument_id TEXT NOT NULL,"
                               "interval TEXT NOT NULL,provider_symbol TEXT NOT NULL,"
                               "currency TEXT NULL,source_url TEXT NULL,attribution_id TEXT NULL,"
                               "retrieved_at TEXT NULL,quality_status INTEGER NOT NULL,"
                               "is_partial INTEGER NOT NULL,is_truncated INTEGER NOT NULL,"
                               "missing_value_counts TEXT NOT NULL,warnings TEXT NOT NULL,"
                               "next_page_cursor TEXT NULL,"
                               "PRIMARY KEY(provider_id,instrument_id,interval))"),
                QStringLiteral("CREATE TABLE market_bars ("
                               "provider_id TEXT NOT NULL,instrument_id TEXT NOT NULL,"
                               "interval TEXT NOT NULL,timestamp_ms INTEGER NOT NULL,"
                               "open REAL NOT NULL,high REAL NOT NULL,low REAL NOT NULL,"
                               "close REAL NOT NULL,adjusted_close REAL NULL,volume REAL NULL,"
                               "PRIMARY KEY(provider_id,instrument_id,interval,timestamp_ms),"
                               "FOREIGN KEY(provider_id,instrument_id,interval) "
                               "REFERENCES market_series(provider_id,instrument_id,interval) "
                               "ON DELETE CASCADE)"),
                QStringLiteral("INSERT INTO market_series VALUES "
                               "('legacy','equity.test','1d','TEST','USD',NULL,NULL,NULL,"
                               "0,0,0,'{}','[]',NULL)"),
                QStringLiteral("INSERT INTO market_bars VALUES "
                               "('legacy','equity.test','1d',1795651200000,10,12,9,11,NULL,NULL)")
            };
            for (const QString& statement : schema) {
                QVERIFY2(query.exec(statement), qPrintable(query.lastError().text()));
            }
            database.close();
        }
        QSqlDatabase::removeDatabase(connectionName);

        {
            SqliteMarketDataStore store(databasePath);
            QString error;
            QVERIFY2(store.open(&error), qPrintable(error));
            const auto loaded = store.loadSeries(QStringLiteral("legacy"),
                                                 QStringLiteral("equity.test"),
                                                 QStringLiteral("1d"), &error);
            QVERIFY2(loaded.has_value(), qPrintable(error));
            QCOMPARE(loaded->bars.size(), qsizetype(1));
            QCOMPARE(loaded->bars.first().close, 11.0);
        }

        const QString verifyConnection = QStringLiteral("legacy-version-check");
        {
            QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                               verifyConnection);
            database.setDatabaseName(databasePath);
            QVERIFY(database.open());
            QSqlQuery query(database);
            QVERIFY(query.exec(QStringLiteral("PRAGMA user_version")));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), 1);
            database.close();
        }
        QSqlDatabase::removeDatabase(verifyConnection);
    }

    void rejectsNewerSchemaVersion() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString databasePath = directory.filePath(QStringLiteral("future.sqlite"));
        const QString connectionName = QStringLiteral("future-schema-seed");
        {
            QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                               connectionName);
            database.setDatabaseName(databasePath);
            QVERIFY(database.open());
            QSqlQuery query(database);
            QVERIFY(query.exec(QStringLiteral("PRAGMA user_version = 99")));
            database.close();
        }
        QSqlDatabase::removeDatabase(connectionName);

        SqliteMarketDataStore store(databasePath);
        QString error;
        QVERIFY(!store.open(&error));
        QVERIFY(!store.isOpen());
        QVERIFY(error.contains(QStringLiteral("newer than supported")));
    }

    void preservesPreviousSeriesWhenReplacementFailsMidTransaction() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString databasePath = directory.filePath(QStringLiteral("atomic-recovery.sqlite"));
        const CanonicalMarketSeries original = sampleSeries();

        {
            SqliteMarketDataStore store(databasePath);
            QString error;
            QVERIFY2(store.open(&error), qPrintable(error));
            QVERIFY2(store.saveSeries(original, &error), qPrintable(error));
        }

        // Inject a deterministic SQLite failure after the replacement header has
        // been updated and old bars deleted, but before the new bar is inserted.
        const QString triggerConnection = QStringLiteral("atomic-recovery-trigger");
        {
            QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                               triggerConnection);
            database.setDatabaseName(databasePath);
            QVERIFY(database.open());
            QSqlQuery query(database);
            const qint64 failingTimestamp = QDateTime::fromString(
                QStringLiteral("2026-11-28T01:05:00.000Z"), Qt::ISODateWithMs)
                                                .toMSecsSinceEpoch();
            const QString triggerSql = QStringLiteral(
                "CREATE TRIGGER reject_replacement_bar BEFORE INSERT ON market_bars "
                "WHEN NEW.timestamp_ms = %1 "
                "BEGIN SELECT RAISE(ABORT, 'forced insert failure'); END")
                                           .arg(failingTimestamp);
            QVERIFY2(query.exec(triggerSql), qPrintable(query.lastError().text()));
            database.close();
        }
        QSqlDatabase::removeDatabase(triggerConnection);

        {
            SqliteMarketDataStore store(databasePath);
            QString error;
            QVERIFY2(store.open(&error), qPrintable(error));

            CanonicalMarketSeries replacement = original;
            replacement.providerSymbol = QStringLiteral("SHOULD_NOT_COMMIT");
            replacement.bars.clear();
            const QDateTime replacementStart = QDateTime::fromString(
                QStringLiteral("2026-11-28T00:00:00.000Z"), Qt::ISODateWithMs);
            for (qsizetype i = 0; i < 70; ++i) {
                CanonicalMarketBar replacementBar = original.bars.last();
                replacementBar.timestamp = replacementStart.addSecs(i * 60);
                replacementBar.open = 200.0 + static_cast<double>(i);
                replacementBar.high = replacementBar.open + 2.0;
                replacementBar.low = replacementBar.open - 2.0;
                replacementBar.close = replacementBar.open + 1.0;
                replacement.bars.append(replacementBar);
            }

            QVERIFY(!store.saveSeries(replacement, &error));
            QVERIFY(!error.isEmpty());

            error.clear();
            const auto loaded = store.loadSeries(original.providerId, original.instrumentId,
                                                 original.interval, &error);
            QVERIFY2(loaded.has_value(), qPrintable(error));
            QCOMPARE(loaded->providerSymbol, original.providerSymbol);
            QCOMPARE(loaded->bars.size(), original.bars.size());
            QCOMPARE(loaded->bars.at(0).close, original.bars.at(0).close);
            QCOMPARE(loaded->bars.at(1).close, original.bars.at(1).close);
            QVERIFY(loaded->validate().isEmpty());
        }
    }


    void usesCompositePrimaryKeyForLargeBoundedRangeReads() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString databasePath = directory.filePath(QStringLiteral("query-performance.sqlite"));
        constexpr qsizetype barCount = 5000;

        CanonicalMarketSeries large;
        large.providerId = QStringLiteral("yahoo_finance");
        large.instrumentId = QStringLiteral("equity.large");
        large.providerSymbol = QStringLiteral("LARGE");
        large.interval = QStringLiteral("1m");
        large.currency = QStringLiteral("USD");
        large.provenance.retrievedAt = QDateTime::fromString(
            QStringLiteral("2026-11-28T00:00:00.000Z"), Qt::ISODateWithMs);
        large.quality.status = DataStatus::Ok;
        large.bars.reserve(barCount);

        const QDateTime firstTimestamp = QDateTime::fromString(
            QStringLiteral("2026-11-01T00:00:00.000Z"), Qt::ISODateWithMs);
        for (qsizetype i = 0; i < barCount; ++i) {
            CanonicalMarketBar bar;
            bar.timestamp = firstTimestamp.addSecs(i * 60);
            bar.open = 100.0 + static_cast<double>(i) * 0.01;
            bar.high = bar.open + 1.0;
            bar.low = bar.open - 1.0;
            bar.close = bar.open + 0.5;
            bar.volume = static_cast<double>(i + 1);
            large.bars.append(bar);
        }
        QVERIFY(large.validate().isEmpty());

        {
            SqliteMarketDataStore store(databasePath);
            QString error;
            QVERIFY2(store.open(&error), qPrintable(error));
            QVERIFY2(store.saveSeries(large, &error), qPrintable(error));

            // Similar keys must remain isolated by the full composite key.
            CanonicalMarketSeries otherInstrument = sampleSeries();
            otherInstrument.instrumentId = QStringLiteral("equity.other");
            otherInstrument.providerSymbol = QStringLiteral("OTHER");
            QVERIFY2(store.saveSeries(otherInstrument, &error), qPrintable(error));

            CanonicalMarketSeries otherInterval = sampleSeries();
            otherInterval.interval = QStringLiteral("1h");
            QVERIFY2(store.saveSeries(otherInterval, &error), qPrintable(error));

            const QDateTime rangeStart = firstTimestamp.addSecs(1000 * 60);
            const QDateTime rangeEnd = firstTimestamp.addSecs(2500 * 60);
            QElapsedTimer timer;
            timer.start();
            const auto bounded = store.loadSeriesRange(
                large.providerId, large.instrumentId, large.interval,
                rangeStart, rangeEnd, &error, 1000);
            const qint64 elapsedMs = timer.elapsed();

            QVERIFY2(bounded.has_value(), qPrintable(error));
            QCOMPARE(bounded->bars.size(), qsizetype(1000));
            QCOMPARE(bounded->bars.first().timestamp, rangeStart);
            QCOMPARE(bounded->bars.last().timestamp, firstTimestamp.addSecs(1999 * 60));
            QVERIFY(bounded->quality.isTruncated);
            QVERIFY(bounded->validate().isEmpty());

            qInfo().noquote() << QStringLiteral(
                "SQLite bounded range benchmark: %1 source bars, 1000 returned, %2 ms")
                .arg(barCount).arg(elapsedMs);

            const auto isolatedInstrument = store.loadSeriesRange(
                otherInstrument.providerId, otherInstrument.instrumentId,
                otherInstrument.interval, otherInstrument.bars.first().timestamp,
                otherInstrument.bars.last().timestamp.addMSecs(1), &error);
            QVERIFY2(isolatedInstrument.has_value(), qPrintable(error));
            QCOMPARE(isolatedInstrument->bars.size(), otherInstrument.bars.size());

            const auto isolatedInterval = store.loadSeriesRange(
                otherInterval.providerId, otherInterval.instrumentId,
                otherInterval.interval, otherInterval.bars.first().timestamp,
                otherInterval.bars.last().timestamp.addMSecs(1), &error);
            QVERIFY2(isolatedInterval.has_value(), qPrintable(error));
            QCOMPARE(isolatedInterval->bars.size(), otherInterval.bars.size());
        }

        const QString connectionName = QStringLiteral("query-plan-check");
        {
            QSqlDatabase database = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), connectionName);
            database.setDatabaseName(databasePath);
            QVERIFY(database.open());

            QSqlQuery plan(database);
            QVERIFY2(plan.prepare(QStringLiteral(
                "EXPLAIN QUERY PLAN SELECT timestamp_ms FROM market_bars "
                "WHERE provider_id=? AND instrument_id=? AND interval=? "
                "AND timestamp_ms>=? AND timestamp_ms<? ORDER BY timestamp_ms ASC")),
                qPrintable(plan.lastError().text()));
            plan.addBindValue(large.providerId);
            plan.addBindValue(large.instrumentId);
            plan.addBindValue(large.interval);
            plan.addBindValue(firstTimestamp.addSecs(1000 * 60).toMSecsSinceEpoch());
            plan.addBindValue(firstTimestamp.addSecs(2500 * 60).toMSecsSinceEpoch());
            QVERIFY2(plan.exec(), qPrintable(plan.lastError().text()));

            QStringList planDetails;
            while (plan.next()) planDetails.append(plan.value(3).toString());
            QVERIFY(!planDetails.isEmpty());
            const QString planText = planDetails.join(QLatin1Char(' ')).toLower();
            QVERIFY2(planText.contains(QStringLiteral("market_bars")),
                     qPrintable(planText));
            QVERIFY2(planText.contains(QStringLiteral("timestamp_ms")),
                     qPrintable(planText));
            QVERIFY2(planText.contains(QStringLiteral("primary key"))
                         || planText.contains(QStringLiteral("autoindex")),
                     qPrintable(planText));

            QSqlQuery integrity(database);
            QVERIFY(integrity.exec(QStringLiteral("PRAGMA integrity_check")));
            QVERIFY(integrity.next());
            QCOMPARE(integrity.value(0).toString(), QStringLiteral("ok"));
            database.close();
        }
        QSqlDatabase::removeDatabase(connectionName);
    }


    void enablesWalForPersistentMarketDataDatabase() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString databasePath = directory.filePath(QStringLiteral("wal-mode.sqlite"));

        {
            SqliteMarketDataStore store(databasePath);
            QString error;
            QVERIFY2(store.open(&error), qPrintable(error));
        }

        const QString connectionName = QStringLiteral("wal-mode-check");
        {
            QSqlDatabase database = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), connectionName);
            database.setDatabaseName(databasePath);
            QVERIFY(database.open());

            QSqlQuery query(database);
            QVERIFY(query.exec(QStringLiteral("PRAGMA journal_mode")));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toString().toLower(), QStringLiteral("wal"));
            database.close();
        }
        QSqlDatabase::removeDatabase(connectionName);
    }

    void walAllowsWriterCommitWhileReaderHoldsSnapshot() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString databasePath = directory.filePath(QStringLiteral("wal-concurrency.sqlite"));
        const CanonicalMarketSeries source = sampleSeries();

        {
            SqliteMarketDataStore store(databasePath);
            QString error;
            QVERIFY2(store.open(&error), qPrintable(error));
            QVERIFY2(store.saveSeries(source, &error), qPrintable(error));
        }

        const QString readerName = QStringLiteral("wal-snapshot-reader");
        const QString writerName = QStringLiteral("wal-snapshot-writer");
        {
            QSqlDatabase reader = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), readerName);
            reader.setDatabaseName(databasePath);
            QVERIFY(reader.open());

            QSqlDatabase writer = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), writerName);
            writer.setDatabaseName(databasePath);
            QVERIFY(writer.open());

            QVERIFY(reader.transaction());
            QSqlQuery establishSnapshot(reader);
            QVERIFY(establishSnapshot.exec(QStringLiteral(
                "SELECT close FROM market_bars WHERE provider_id='yahoo_finance' "
                "AND instrument_id='equity.aapl' AND interval='1d' "
                "ORDER BY timestamp_ms LIMIT 1")));
            QVERIFY(establishSnapshot.next());
            QCOMPARE(establishSnapshot.value(0).toDouble(), 103.0);
            establishSnapshot.finish();

            // A committed write must not wait for the active reader to finish.
            QVERIFY(writer.transaction());
            QSqlQuery update(writer);
            QVERIFY(update.exec(QStringLiteral(
                "UPDATE market_series SET provider_symbol='AAPL_UPDATED' "
                "WHERE provider_id='yahoo_finance' AND instrument_id='equity.aapl' "
                "AND interval='1d'")));
            QVERIFY2(writer.commit(), qPrintable(writer.lastError().text()));

            // The reader continues to see its original consistent snapshot.
            QSqlQuery snapshotValue(reader);
            QVERIFY(snapshotValue.exec(QStringLiteral(
                "SELECT provider_symbol FROM market_series "
                "WHERE provider_id='yahoo_finance' AND instrument_id='equity.aapl' "
                "AND interval='1d'")));
            QVERIFY(snapshotValue.next());
            QCOMPARE(snapshotValue.value(0).toString(), source.providerSymbol);
            snapshotValue.finish();
            QVERIFY(reader.commit());

            QSqlQuery latest(writer);
            QVERIFY(latest.exec(QStringLiteral(
                "SELECT provider_symbol FROM market_series "
                "WHERE provider_id='yahoo_finance' AND instrument_id='equity.aapl' "
                "AND interval='1d'")));
            QVERIFY(latest.next());
            QCOMPARE(latest.value(0).toString(), QStringLiteral("AAPL_UPDATED"));

            reader.close();
            writer.close();
        }
        QSqlDatabase::removeDatabase(readerName);
        QSqlDatabase::removeDatabase(writerName);
    }

    void writesSeriesAcrossMultipleInsertBatches() {
        SqliteMarketDataStore store(QStringLiteral(":memory:"));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));

        CanonicalMarketSeries source = sampleSeries();
        source.bars.clear();
        const QDateTime firstTimestamp = QDateTime::fromString(
            QStringLiteral("2026-11-20T00:00:00.000Z"), Qt::ISODateWithMs);
        constexpr qsizetype barCount = 125;
        for (qsizetype i = 0; i < barCount; ++i) {
            CanonicalMarketBar bar;
            bar.timestamp = firstTimestamp.addSecs(i * 60);
            bar.open = 100.0 + static_cast<double>(i);
            bar.high = bar.open + 2.0;
            bar.low = bar.open - 2.0;
            bar.close = bar.open + 1.0;
            if (i % 2 == 0) {
                bar.adjustedClose = bar.close - 0.25;
                bar.volume = static_cast<double>(i * 10);
            }
            source.bars.append(bar);
        }
        QVERIFY(source.validate().isEmpty());
        QVERIFY2(store.saveSeries(source, &error), qPrintable(error));

        const auto loaded = store.loadSeries(source.providerId, source.instrumentId,
                                             source.interval, &error);
        QVERIFY2(loaded.has_value(), qPrintable(error));
        QCOMPARE(loaded->bars.size(), barCount);
        QCOMPARE(loaded->bars.first().timestamp, firstTimestamp);
        QCOMPARE(loaded->bars.last().timestamp, firstTimestamp.addSecs((barCount - 1) * 60));
        QCOMPARE(loaded->bars.at(64).close, source.bars.at(64).close);
        QCOMPARE(loaded->bars.at(124).close, source.bars.at(124).close);
        QVERIFY(!loaded->bars.at(65).adjustedClose.has_value());
        QVERIFY(!loaded->bars.at(65).volume.has_value());
        QVERIFY(loaded->validate().isEmpty());
    }

    void benchmarksLargeSeriesBatchPersistence() {
        SqliteMarketDataStore store(QStringLiteral(":memory:"));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));

        CanonicalMarketSeries source = sampleSeries();
        source.instrumentId = QStringLiteral("equity.batch-benchmark");
        source.providerSymbol = QStringLiteral("BATCH");
        source.interval = QStringLiteral("1m");
        source.bars.clear();

        const QDateTime firstTimestamp = QDateTime::fromString(
            QStringLiteral("2026-11-01T00:00:00.000Z"), Qt::ISODateWithMs);
        constexpr qsizetype barCount = 10000;
        source.bars.reserve(barCount);
        for (qsizetype i = 0; i < barCount; ++i) {
            CanonicalMarketBar bar;
            bar.timestamp = firstTimestamp.addSecs(i * 60);
            bar.open = 100.0 + static_cast<double>(i % 1000) * 0.01;
            bar.high = bar.open + 1.0;
            bar.low = bar.open - 1.0;
            bar.close = bar.open + 0.25;
            if (i % 3 != 0) {
                bar.adjustedClose = bar.close - 0.05;
                bar.volume = static_cast<double>(i + 1);
            }
            source.bars.append(bar);
        }
        QVERIFY(source.validate().isEmpty());

        QElapsedTimer timer;
        timer.start();
        QVERIFY2(store.saveSeries(source, &error), qPrintable(error));
        const qint64 saveElapsedMs = timer.elapsed();

        const auto loaded = store.loadSeries(source.providerId, source.instrumentId,
                                             source.interval, &error);
        QVERIFY2(loaded.has_value(), qPrintable(error));
        QCOMPARE(loaded->bars.size(), barCount);
        QCOMPARE(loaded->bars.first().timestamp, firstTimestamp);
        QCOMPARE(loaded->bars.last().timestamp,
                 firstTimestamp.addSecs((barCount - 1) * 60));
        QCOMPARE(loaded->bars.at(9999).close, source.bars.at(9999).close);
        QCOMPARE(loaded->bars.at(9999).volume, source.bars.at(9999).volume);
        QVERIFY(loaded->validate().isEmpty());

        const double barsPerSecond = saveElapsedMs > 0
            ? static_cast<double>(barCount) * 1000.0 / static_cast<double>(saveElapsedMs)
            : 0.0;
        qInfo().noquote() << QStringLiteral(
            "SQLite batch write benchmark: %1 bars in %2 ms (%3 bars/s)")
            .arg(barCount).arg(saveElapsedMs).arg(barsPerSecond, 0, 'f', 0);
    }

    void rejectsInvalidSeriesWithoutWriting() {
        SqliteMarketDataStore store(QStringLiteral(":memory:"));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));

        CanonicalMarketSeries invalid;
        QVERIFY(!store.saveSeries(invalid, &error));
        QVERIFY(!error.isEmpty());
        error.clear();
        QVERIFY(!store.loadSeries(QStringLiteral("missing"), QStringLiteral("missing"),
                                  QStringLiteral("1d"), &error).has_value());
        QVERIFY(error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(SqliteMarketDataStoreTest)

#include "tst_sqlite_market_data_store.moc"
