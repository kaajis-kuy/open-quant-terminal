#include "core/providers/FredRequestBuilder.h"

#include <QUrlQuery>
#include <QtTest>

using namespace oqt::providers;

class FredRequestBuilderTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsDefaultObservationRequest() {
        FredObservationRequest input;
        input.apiKey = "test-secret";
        input.seriesId = "GDPC1";

        const auto result = FredRequestBuilder::observations(input);
        QVERIFY2(result.isValid(), qPrintable(result.error));
        QCOMPARE(result.url.host(), QStringLiteral("api.stlouisfed.org"));
        QCOMPARE(result.url.path(), QStringLiteral("/fred/series/observations"));

        const QUrlQuery query(result.url);
        QCOMPARE(query.queryItemValue("file_type"), QStringLiteral("json"));
        QCOMPARE(query.queryItemValue("series_id"), QStringLiteral("GDPC1"));
        QCOMPARE(query.queryItemValue("limit"), QStringLiteral("100000"));
        QCOMPARE(query.queryItemValue("offset"), QStringLiteral("0"));
        QCOMPARE(query.queryItemValue("sort_order"), QStringLiteral("asc"));
    }

    void includesOptionalFiltersAndPagination() {
        FredObservationRequest input;
        input.apiKey = "test-secret";
        input.seriesId = "CPIAUCSL";
        input.observationStart = QDate(2020, 1, 1);
        input.observationEnd = QDate(2025, 12, 31);
        input.units = "pch";
        input.frequency = "q";
        input.aggregationMethod = "avg";
        input.outputType = 4;
        input.limit = 5000;
        input.offset = 1000;

        const auto result = FredRequestBuilder::observations(input);
        QVERIFY2(result.isValid(), qPrintable(result.error));
        const QUrlQuery query(result.url);
        QCOMPARE(query.queryItemValue("observation_start"), QStringLiteral("2020-01-01"));
        QCOMPARE(query.queryItemValue("observation_end"), QStringLiteral("2025-12-31"));
        QCOMPARE(query.queryItemValue("units"), QStringLiteral("pch"));
        QCOMPARE(query.queryItemValue("frequency"), QStringLiteral("q"));
        QCOMPARE(query.queryItemValue("aggregation_method"), QStringLiteral("avg"));
        QCOMPARE(query.queryItemValue("output_type"), QStringLiteral("4"));
        QCOMPARE(query.queryItemValue("offset"), QStringLiteral("1000"));
    }

    void redactsApiKeyFromDiagnosticUrl() {
        FredObservationRequest input;
        input.apiKey = "must-never-appear";
        input.seriesId = "GDP";

        const auto result = FredRequestBuilder::observations(input);
        QVERIFY(result.isValid());
        QVERIFY(!result.redactedUrl.contains("must-never-appear"));
        QVERIFY(result.redactedUrl.contains("%5BREDACTED%5D")
                || result.redactedUrl.contains("[REDACTED]"));
    }

    void rejectsMissingCredentialsAndSeries() {
        FredObservationRequest input;
        input.seriesId = "GDP";
        QVERIFY(!FredRequestBuilder::observations(input).isValid());

        input.apiKey = "test-secret";
        input.seriesId.clear();
        QVERIFY(!FredRequestBuilder::observations(input).isValid());
    }

    void rejectsInvalidRangesAndLimits() {
        FredObservationRequest input;
        input.apiKey = "test-secret";
        input.seriesId = "GDP";
        input.observationStart = QDate(2025, 1, 1);
        input.observationEnd = QDate(2020, 1, 1);
        QVERIFY(!FredRequestBuilder::observations(input).isValid());

        input.observationStart.reset();
        input.observationEnd.reset();
        input.limit = 100001;
        QVERIFY(!FredRequestBuilder::observations(input).isValid());

        input.limit = 10;
        input.offset = -1;
        QVERIFY(!FredRequestBuilder::observations(input).isValid());
    }

    void rejectsInvalidDateAndSortOrder() {
        FredObservationRequest input;
        input.apiKey = "test-secret";
        input.seriesId = "GDP";
        input.realtimeStart = "2026-13-40";
        QVERIFY(!FredRequestBuilder::observations(input).isValid());

        input.realtimeStart.clear();
        input.sortOrder = "sideways";
        QVERIFY(!FredRequestBuilder::observations(input).isValid());
    }
};

QTEST_GUILESS_MAIN(FredRequestBuilderTest)

#include "tst_fred_request_builder.moc"
