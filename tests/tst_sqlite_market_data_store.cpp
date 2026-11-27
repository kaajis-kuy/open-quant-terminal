#include "core/storage/SqliteMarketDataStore.h"

#include <QtTest>

#include <QFile>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

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
