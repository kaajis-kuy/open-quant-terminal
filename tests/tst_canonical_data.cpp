#include "core/data/CanonicalData.h"

#include <QtTest>

using namespace oqt::data;

class CanonicalDataTest final : public QObject {
    Q_OBJECT

private:
    static CanonicalObservation validObservation() {
        CanonicalObservation observation;
        observation.providerId = QStringLiteral("fred");
        observation.datasetId = QStringLiteral("gdp");
        observation.seriesId = QStringLiteral("GDPC1");
        observation.period = QDate(2025, 1, 1);
        observation.value = 23000.5;
        observation.unit = QStringLiteral("billions of chained 2017 dollars");
        observation.frequency = QStringLiteral("quarterly");
        observation.geography = QStringLiteral("US");
        return observation;
    }

private slots:
    void validObservation_passes() {
        QVERIFY(validObservation().validate().isEmpty());
    }

    void missingObservationValue_isAllowed() {
        auto observation = validObservation();
        observation.value.reset();

        QVERIFY(observation.validate().isEmpty());
    }

    void nonFiniteObservationValue_isRejected() {
        auto observation = validObservation();
        observation.value = std::numeric_limits<double>::infinity();

        QVERIFY(!observation.validate().isEmpty());
    }

    void invalidScaleMultiplier_isRejected() {
        auto observation = validObservation();
        observation.scaleMultiplier = 0.0;

        QVERIFY(!observation.validate().isEmpty());
    }

    void datasetRequiresMatchingObservationIdentity() {
        CanonicalDataset dataset;
        dataset.providerId = QStringLiteral("fred");
        dataset.datasetId = QStringLiteral("gdp");
        dataset.observations.append(validObservation());

        QVERIFY(dataset.validate().isEmpty());

        auto mismatched = validObservation();
        mismatched.providerId = QStringLiteral("other-provider");
        dataset.observations.append(mismatched);

        QVERIFY(!dataset.validate().isEmpty());
    }

    void qualityRejectsOutOfRangeCompleteness() {
        DataQuality quality;
        quality.completeness = 1.5;

        QVERIFY(!quality.validate().isEmpty());
    }

    void partialStatusRequiresPartialFlag() {
        DataQuality quality;
        quality.status = DataStatus::Partial;

        QVERIFY(!quality.validate().isEmpty());

        quality.isPartial = true;
        QVERIFY(quality.validate().isEmpty());
    }

    void staleProvenanceRequiresCache() {
        Provenance provenance;
        provenance.stale = true;

        QVERIFY(!provenance.validate().isEmpty());

        provenance.servedFromCache = true;
        QVERIFY(provenance.validate().isEmpty());
    }

    void rawResultRequiresPayloadOrReference() {
        ProviderRawResult result;
        result.providerId = QStringLiteral("fred");
        result.operationId = QStringLiteral("series");
        result.responseSchemaVersion = QStringLiteral("1");
        result.fetchedAt = QDateTime::currentDateTimeUtc();

        QVERIFY(!result.validate().isEmpty());

        result.rawPayload = QJsonObject{{QStringLiteral("observations"), QJsonArray{}}};
        QVERIFY(result.validate().isEmpty());
    }
};

QTEST_GUILESS_MAIN(CanonicalDataTest)

#include "tst_canonical_data.moc"
