#include "core/services/EconomicDataIngestionService.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QtTest>

using namespace oqt::data;
using namespace oqt::providers;
using namespace oqt::services;
using namespace oqt::storage;

namespace {

AdapterResult sampleResult(double value, bool includeMissing = true) {
    CanonicalDataset dataset;
    dataset.providerId = QStringLiteral("fred");
    dataset.datasetId = QStringLiteral("fred.gdp");
    dataset.schemaVersion = QStringLiteral("1.0");
    dataset.provenance.sourceUrl =
        QStringLiteral("https://api.stlouisfed.org/fred/series/observations");
    dataset.provenance.attributionId = QStringLiteral("FRED");
    dataset.provenance.retrievedAt = QDateTime::fromString(
        QStringLiteral("2026-11-28T00:00:00.000Z"), Qt::ISODateWithMs);
    dataset.quality.status = DataStatus::Partial;
    dataset.quality.isPartial = true;
    dataset.quality.missingValueCounts.insert(QStringLiteral("fred_missing_or_non_numeric"), 1);
    dataset.quality.warnings.append(QStringLiteral("fixture partial result"));

    CanonicalObservation observation;
    observation.providerId = dataset.providerId;
    observation.datasetId = dataset.datasetId;
    observation.seriesId = QStringLiteral("GDP");
    observation.period = QDate(2025, 1, 1);
    observation.value = value;
    observation.unit = QStringLiteral("billions of dollars");
    observation.frequency = QStringLiteral("Quarterly");
    dataset.observations.append(observation);

    if (includeMissing) {
        CanonicalObservation missing = observation;
        missing.period = QDate(2025, 4, 1);
        missing.value.reset();
        dataset.observations.append(missing);
    }

    AdapterResult result;
    result.responseStatus = ResponseStatus::Partial;
    result.dataset = dataset;
    result.rawResult.providerId = QStringLiteral("fred");
    result.rawResult.operationId = QStringLiteral("observations");
    result.rawResult.requestId = QStringLiteral("request-001");
    result.rawResult.responseSchemaVersion = QStringLiteral("1.0");
    result.rawResult.fetchedAt = dataset.provenance.retrievedAt;
    result.rawResult.rawPayload = QJsonObject{
        {QStringLiteral("observations"), QJsonArray{
            QJsonObject{{QStringLiteral("date"), QStringLiteral("2025-01-01")},
                        {QStringLiteral("value"), QString::number(value)}}
        }}
    };
    result.rawResult.redactedRequestArguments = {
        QStringLiteral("series_id=GDP"), QStringLiteral("api_key=[REDACTED]")};
    return result;
}

} // namespace

class EconomicDataIngestionServiceTest final : public QObject {
    Q_OBJECT

private slots:
    void verifiesHealthyEconomicDatabaseIntegrity() {
        SqliteCanonicalDatasetStore store(QStringLiteral(":memory:"));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));
        EconomicDataIngestionService ingestion(store);
        QVERIFY2(ingestion.persist(sampleResult(123.45), &error), qPrintable(error));
        QVERIFY2(store.verifyIntegrity(&error), qPrintable(error));
        QVERIFY(error.isEmpty());
    }

    void createsConsistentEconomicDatabaseBackup() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString databasePath = directory.filePath(QStringLiteral("economic-source.sqlite"));
        const QString backupPath = directory.filePath(QStringLiteral("economic-backup.sqlite"));

        SqliteCanonicalDatasetStore sourceStore(databasePath);
        QString error;
        QVERIFY2(sourceStore.open(&error), qPrintable(error));
        EconomicDataIngestionService ingestion(sourceStore);
        const AdapterResult source = sampleResult(123.45);
        QVERIFY2(ingestion.persist(source, &error), qPrintable(error));

        QVERIFY2(sourceStore.backupTo(backupPath, &error), qPrintable(error));
        QVERIFY(QFile::exists(backupPath));

        error.clear();
        QVERIFY(!sourceStore.backupTo(databasePath, &error));
        QVERIFY(error.contains(QStringLiteral("must differ")));
        error.clear();
        QVERIFY(!sourceStore.backupTo(backupPath, &error));
        QVERIFY(error.contains(QStringLiteral("already exists")));

        SqliteCanonicalDatasetStore backupStore(backupPath);
        error.clear();
        QVERIFY2(backupStore.open(&error), qPrintable(error));
        QVERIFY2(backupStore.verifyIntegrity(&error), qPrintable(error));
        const auto restored = backupStore.loadDataset(
            QStringLiteral("fred"), QStringLiteral("fred.gdp"), &error);
        QVERIFY2(restored.has_value(), qPrintable(error));
        QCOMPARE(restored->observations.size(), qsizetype(2));
        QCOMPARE(*restored->observations.at(0).value, 123.45);
        QVERIFY(!restored->observations.at(1).value.has_value());
        QCOMPARE(restored->provenance.attributionId, QStringLiteral("FRED"));
    }

    void rejectsEconomicBackupWhenDatabaseIsClosed() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SqliteCanonicalDatasetStore store(
            directory.filePath(QStringLiteral("closed-economic.sqlite")));
        QString error;
        QVERIFY(!store.backupTo(
            directory.filePath(QStringLiteral("economic-backup.sqlite")), &error));
        QVERIFY(error.contains(QStringLiteral("not open")));
    }

    void detectsEconomicForeignKeyViolationsDuringIntegrityVerification() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString databasePath = directory.filePath(QStringLiteral("economic-fk-check.sqlite"));

        {
            SqliteCanonicalDatasetStore store(databasePath);
            QString error;
            QVERIFY2(store.open(&error), qPrintable(error));
        }

        const QString connectionName = QStringLiteral("economic-foreign-key-violation-seed");
        {
            QSqlDatabase database = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), connectionName);
            database.setDatabaseName(databasePath);
            QVERIFY(database.open());
            QSqlQuery query(database);
            QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys = OFF")));
            QVERIFY2(query.exec(QStringLiteral(
                "INSERT INTO economic_observations (provider_id,dataset_id,series_id,period,value,"
                "unit,frequency,geography,seasonal_adjustment,measure_type,currency,base_currency,"
                "quote_currency,scale_multiplier,denominator) VALUES "
                "('orphan','missing.dataset','series','2025-01-01',NULL,'units','','','','',"
                "'','','',1.0,'')")), qPrintable(query.lastError().text()));
            database.close();
        }
        QSqlDatabase::removeDatabase(connectionName);

        SqliteCanonicalDatasetStore store(databasePath);
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));
        QVERIFY(!store.verifyIntegrity(&error));
        QVERIFY(error.contains(QStringLiteral("foreign-key violation")));
        QVERIFY(error.contains(QStringLiteral("economic_observations")));
    }

    void pagesEconomicObservationsInStableDatabaseOrder() {
        SqliteCanonicalDatasetStore store(QStringLiteral(":memory:"));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));
        EconomicDataIngestionService ingestion(store);
        QVERIFY2(ingestion.persist(sampleResult(123.45), &error), qPrintable(error));

        const auto first = store.loadDatasetPage(QStringLiteral("fred"),
            QStringLiteral("fred.gdp"), 1, 0, &error);
        QVERIFY2(first.has_value(), qPrintable(error));
        QCOMPARE(first->dataset.observations.size(), qsizetype(1));
        QCOMPARE(first->dataset.observations.first().period, QDate(2025, 4, 1));
        QVERIFY(first->hasMore);
        QCOMPARE(first->offset, qint64(0));

        const auto second = store.loadDatasetPage(QStringLiteral("fred"),
            QStringLiteral("fred.gdp"), 1, 1, &error);
        QVERIFY2(second.has_value(), qPrintable(error));
        QCOMPARE(second->dataset.observations.size(), qsizetype(1));
        QCOMPARE(second->dataset.observations.first().period, QDate(2025, 1, 1));
        QVERIFY(!second->hasMore);

        const auto empty = store.loadDatasetPage(QStringLiteral("fred"),
            QStringLiteral("fred.gdp"), 1, 2, &error);
        QVERIFY2(empty.has_value(), qPrintable(error));
        QVERIFY(empty->dataset.observations.isEmpty());
        QVERIFY(!empty->hasMore);

        error.clear();
        QVERIFY(!store.loadDatasetPage(QStringLiteral("fred"),
            QStringLiteral("fred.gdp"), 0, 0, &error));
        QVERIFY(error.contains(QStringLiteral("pageSize")));
        error.clear();
        QVERIFY(!store.loadDatasetPage(QStringLiteral("fred"),
            QStringLiteral("fred.gdp"), 1, -1, &error));
        QVERIFY(error.contains(QStringLiteral("offset")));
    }

    void persistsAndReloadsDatasetIncludingMissingValues() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("economic.sqlite"));

        {
            SqliteCanonicalDatasetStore store(path);
            QString error;
            QVERIFY2(store.open(&error), qPrintable(error));
            EconomicDataIngestionService ingestion(store);

            const AdapterResult result = sampleResult(123.45);
            QVERIFY2(ingestion.persist(result, &error), qPrintable(error));

            const auto loaded = store.loadDataset(
                QStringLiteral("fred"), QStringLiteral("fred.gdp"), &error);
            QVERIFY2(loaded.has_value(), qPrintable(error));
            QCOMPARE(loaded->observations.size(), qsizetype(2));
            QCOMPARE(*loaded->observations.at(0).value, 123.45);
            QVERIFY(!loaded->observations.at(0).geography.isNull());
            QCOMPARE(loaded->observations.at(0).geography, QStringLiteral(""));
            QVERIFY(!loaded->observations.at(1).value.has_value());
            QCOMPARE(loaded->quality.status, DataStatus::Partial);
            QVERIFY(loaded->quality.isPartial);
            QCOMPARE(loaded->quality.missingValueCounts.value(
                QStringLiteral("fred_missing_or_non_numeric")), qsizetype(1));
            QCOMPARE(loaded->provenance.attributionId, QStringLiteral("FRED"));
            QVERIFY(loaded->validate().isEmpty());
        }

        // A fresh connection proves the rows are durable, not only in-memory.
        SqliteCanonicalDatasetStore reopened(path);
        QString error;
        QVERIFY2(reopened.open(&error), qPrintable(error));
        const auto loaded = reopened.loadDataset(
            QStringLiteral("fred"), QStringLiteral("fred.gdp"), &error);
        QVERIFY2(loaded.has_value(), qPrintable(error));
        QCOMPARE(loaded->observations.size(), qsizetype(2));
    }

    void upsertUpdatesExistingObservationWithoutDuplicates() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SqliteCanonicalDatasetStore store(
            directory.filePath(QStringLiteral("economic.sqlite")));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));
        EconomicDataIngestionService ingestion(store);

        QVERIFY2(ingestion.persist(sampleResult(100.0), &error), qPrintable(error));
        QVERIFY2(ingestion.persist(sampleResult(200.0), &error), qPrintable(error));

        const auto loaded = store.loadDataset(
            QStringLiteral("fred"), QStringLiteral("fred.gdp"), &error);
        QVERIFY2(loaded.has_value(), qPrintable(error));
        QCOMPARE(loaded->observations.size(), qsizetype(2));
        QCOMPARE(*loaded->observations.at(0).value, 200.0);
    }

    void rejectsInvalidAndMarketSeriesResults() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SqliteCanonicalDatasetStore store(
            directory.filePath(QStringLiteral("economic.sqlite")));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));
        EconomicDataIngestionService ingestion(store);

        AdapterResult invalid;
        invalid.responseStatus = ResponseStatus::Failed;
        invalid.errors.append(QStringLiteral("provider failed"));
        QVERIFY(!ingestion.persist(invalid, &error));
        QVERIFY(!error.isEmpty());
    }

    void enablesWalForFileBackedEconomicDatabase() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("wal-mode.sqlite"));

        {
            SqliteCanonicalDatasetStore store(path);
            QString error;
            QVERIFY2(store.open(&error), qPrintable(error));
            QVERIFY(store.isOpen());
        }

        const QString connectionName = QStringLiteral("economic-wal-mode-check");
        {
            QSqlDatabase database = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), connectionName);
            database.setDatabaseName(path);
            QVERIFY(database.open());
            QSqlQuery query(database);
            QVERIFY2(query.exec(QStringLiteral("PRAGMA journal_mode")),
                     qPrintable(query.lastError().text()));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toString().toLower(), QStringLiteral("wal"));
            database.close();
        }
        QSqlDatabase::removeDatabase(connectionName);

        // Reopening after the last connection closes must retain the file and WAL mode.
        SqliteCanonicalDatasetStore reopened(path);
        QString error;
        QVERIFY2(reopened.open(&error), qPrintable(error));
        QVERIFY(reopened.isOpen());
    }

    void listsPersistedEconomicDatasetsWithAggregatesAndFiltering() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        SqliteCanonicalDatasetStore store(QStringLiteral(":memory:"));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));
        EconomicDataIngestionService ingestion(store);
        QVERIFY2(ingestion.persist(sampleResult(123.45), &error), qPrintable(error));

        const auto page = store.loadDatasetCatalog({}, 10, 0, &error);
        QVERIFY2(page.has_value(), qPrintable(error));
        QCOMPARE(page->entries.size(), qsizetype(1));
        const auto& entry = page->entries.first();
        QCOMPARE(entry.providerId, QStringLiteral("fred"));
        QCOMPARE(entry.datasetId, QStringLiteral("fred.gdp"));
        QCOMPARE(entry.observationCount, qint64(2));
        QCOMPARE(entry.earliestPeriod, QDate(2025, 1, 1));
        QCOMPARE(entry.latestPeriod, QDate(2025, 4, 1));
        QCOMPARE(entry.attributionId, QStringLiteral("FRED"));

        const auto filtered = store.loadDatasetCatalog(QStringLiteral("gdp"), 10, 0, &error);
        QVERIFY2(filtered.has_value(), qPrintable(error));
        QCOMPARE(filtered->entries.size(), qsizetype(1));
        const auto empty = store.loadDatasetCatalog(QStringLiteral("missing"), 10, 0, &error);
        QVERIFY2(empty.has_value(), qPrintable(error));
        QVERIFY(empty->entries.isEmpty());

        error.clear();
        QVERIFY(!store.loadDatasetCatalog({}, 501, 0, &error));
        QVERIFY(error.contains(QStringLiteral("page size")));
    }

    void recoversAfterSchemaInitializationFailure() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("recovery.sqlite"));
        const QString setupConnection = QStringLiteral("economic-schema-recovery-setup");

        {
            QSqlDatabase database = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), setupConnection);
            database.setDatabaseName(path);
            QVERIFY(database.open());
            QSqlQuery query(database);
            QVERIFY2(query.exec(QStringLiteral("PRAGMA user_version = 99")),
                     qPrintable(query.lastError().text()));
            database.close();
        }
        QSqlDatabase::removeDatabase(setupConnection);

        SqliteCanonicalDatasetStore store(path);
        QString error;
        QVERIFY(!store.open(&error));
        QVERIFY(error.contains(QStringLiteral("newer than supported")));
        QVERIFY(!store.isOpen());

        const QString repairConnection = QStringLiteral("economic-schema-recovery-repair");
        {
            QSqlDatabase database = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), repairConnection);
            database.setDatabaseName(path);
            QVERIFY(database.open());
            QSqlQuery query(database);
            QVERIFY2(query.exec(QStringLiteral("PRAGMA user_version = 0")),
                     qPrintable(query.lastError().text()));
            database.close();
        }
        QSqlDatabase::removeDatabase(repairConnection);

        error.clear();
        QVERIFY2(store.open(&error), qPrintable(error));
        QVERIFY(store.isOpen());
        const auto loaded = store.loadDataset(
            QStringLiteral("fred"), QStringLiteral("fred.gdp"), &error);
        QVERIFY(!loaded.has_value());
        QVERIFY(error.isEmpty());
    }

    void rollsBackDatasetAndObservationsWhenWriteFailsMidTransaction() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("atomic-recovery.sqlite"));

        {
            SqliteCanonicalDatasetStore store(path);
            QString error;
            QVERIFY2(store.open(&error), qPrintable(error));
            EconomicDataIngestionService ingestion(store);
            QVERIFY2(ingestion.persist(sampleResult(123.45), &error), qPrintable(error));
        }

        const QString triggerConnection = QStringLiteral("economic-rollback-trigger");
        {
            QSqlDatabase database = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), triggerConnection);
            database.setDatabaseName(path);
            QVERIFY(database.open());
            QSqlQuery query(database);
            const QString triggerSql = QStringLiteral(
                "CREATE TRIGGER reject_economic_observation "
                "BEFORE INSERT ON economic_observations "
                "WHEN NEW.period = '2025-04-01' "
                "BEGIN SELECT RAISE(ABORT, 'forced insert failure'); END");
            QVERIFY2(query.exec(triggerSql), qPrintable(query.lastError().text()));
            database.close();
        }
        QSqlDatabase::removeDatabase(triggerConnection);

        {
            SqliteCanonicalDatasetStore store(path);
            QString error;
            QVERIFY2(store.open(&error), qPrintable(error));
            EconomicDataIngestionService ingestion(store);
            QVERIFY(!ingestion.persist(sampleResult(200.0), &error));
            QVERIFY(!error.isEmpty());

            error.clear();
            const auto loaded = store.loadDataset(
                QStringLiteral("fred"), QStringLiteral("fred.gdp"), &error);
            QVERIFY2(loaded.has_value(), qPrintable(error));
            QCOMPARE(loaded->observations.size(), qsizetype(2));
            QCOMPARE(*loaded->observations.at(0).value, 123.45);
        }
    }
};

QTEST_GUILESS_MAIN(EconomicDataIngestionServiceTest)

#include "tst_economic_data_ingestion_service.moc"
