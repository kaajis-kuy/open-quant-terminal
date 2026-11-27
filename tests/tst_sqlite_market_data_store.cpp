#include "core/storage/SqliteMarketDataStore.h"

#include <QtTest>

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
        QVERIFY(limited->validate().isEmpty());

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
                QStringLiteral("2026-11-28T00:00:00.000Z"), Qt::ISODateWithMs)
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
            CanonicalMarketBar replacementBar = original.bars.last();
            replacementBar.timestamp = QDateTime::fromString(
                QStringLiteral("2026-11-28T00:00:00.000Z"), Qt::ISODateWithMs);
            replacementBar.close = 999.0;
            replacement.bars.append(replacementBar);

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
