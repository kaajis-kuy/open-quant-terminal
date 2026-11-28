#include "core/services/EconomicDataIngestionService.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
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
};

QTEST_GUILESS_MAIN(EconomicDataIngestionServiceTest)

#include "tst_economic_data_ingestion_service.moc"
