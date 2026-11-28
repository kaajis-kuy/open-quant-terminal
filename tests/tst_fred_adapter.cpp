#include "core/providers/FredAdapter.h"

#include <QtTest>

using namespace oqt::providers;

namespace {

ProviderRequestContext fredContext() {
    ProviderRequestContext context;
    context.providerId = QStringLiteral("fred");
    context.operationId = QStringLiteral("observations");
    context.datasetId = QStringLiteral("fred.gdp");
    context.requestId = QStringLiteral("fixture-001");
    context.requestedAt = QDateTime::fromString(
        QStringLiteral("2026-01-15T12:00:00Z"), Qt::ISODate);
    context.redactedParameters.insert(QStringLiteral("series_id"), QStringLiteral("GDP"));
    context.redactedParameters.insert(QStringLiteral("units"), QStringLiteral(" Billions of Dollars "));
    context.redactedParameters.insert(QStringLiteral("frequency"), QStringLiteral("Quarterly"));
    return context;
}

ProviderResponse responseFrom(const QByteArray& json) {
    return ProviderResponse::decode(json);
}

} // namespace

class FredAdapterTest final : public QObject {
    Q_OBJECT

private slots:
    void providerIdentityAndOperations() {
        FredAdapter adapter;
        QCOMPARE(adapter.providerId(), QStringLiteral("fred"));
        QVERIFY(adapter.supportsOperation(QStringLiteral("observations")));
        QVERIFY(!adapter.supportsOperation(QStringLiteral("series")));
    }

    void normalizesNumericObservations() {
        FredAdapter adapter;
        const auto response = responseFrom(R"({
            "realtime_start":"2026-01-15",
            "realtime_end":"2026-01-15",
            "observation_start":"2020-01-01",
            "observation_end":"2020-12-31",
            "units":"lin",
            "output_type":1,
            "file_type":"json",
            "order_by":"observation_date",
            "sort_order":"asc",
            "count":2,
            "offset":0,
            "limit":100000,
            "observations":[
                {"realtime_start":"2026-01-15","realtime_end":"2026-01-15","date":"2020-01-01","value":"21000.5"},
                {"realtime_start":"2026-01-15","realtime_end":"2026-01-15","date":"2020-04-01","value":"21500"}
            ]
        })");

        const auto result = adapter.normalize(response, fredContext());
        QVERIFY2(result.errors.isEmpty(), qPrintable(result.errors.join("; ")));
        QVERIFY(result.isValid());
        QCOMPARE(result.dataset->observations.size(), qsizetype(2));
        QCOMPARE(result.dataset->observations.at(0).period, QDate(2020, 1, 1));
        QCOMPARE(*result.dataset->observations.at(0).value, 21000.5);
        QCOMPARE(result.dataset->observations.at(0).seriesId, QStringLiteral("GDP"));
        QCOMPARE(result.dataset->observations.at(0).unit, QStringLiteral("Billions of Dollars"));
        QCOMPARE(result.dataset->quality.status, oqt::data::DataStatus::Ok);
    }

    void fredDotIsPreservedAsMissingValue() {
        FredAdapter adapter;
        const auto response = responseFrom(R"({
            "count":1,"offset":0,"limit":100000,
            "observations":[{"date":"2020-01-01","value":"."}]
        })");
        const auto result = adapter.normalize(response, fredContext());
        QVERIFY2(result.errors.isEmpty(), qPrintable(result.errors.join("; ")));
        QVERIFY(result.isValid());
        QVERIFY(!result.dataset->observations.at(0).value.has_value());
        QCOMPARE(result.dataset->quality.missingValueCounts.value(
            QStringLiteral("fred_missing_or_non_numeric")), qsizetype(1));
        QCOMPARE(result.dataset->quality.status, oqt::data::DataStatus::Ok);
    }

    void detectsPaginationFromFredCountAndOffset() {
        FredAdapter adapter;
        const auto response = responseFrom(R"({
            "count":3,"offset":0,"limit":2,
            "observations":[
                {"date":"2020-01-01","value":"1"},
                {"date":"2020-02-01","value":"2"}
            ]
        })");
        const auto result = adapter.normalize(response, fredContext());
        QVERIFY2(result.errors.isEmpty(), qPrintable(result.errors.join("; ")));
        QVERIFY(result.isValid());
        QCOMPARE(result.dataset->quality.status, oqt::data::DataStatus::Partial);
        QVERIFY(result.dataset->quality.isTruncated);
        QCOMPARE(result.dataset->quality.nextPageCursor, QStringLiteral("offset=2"));
    }

    void skipsMalformedObservationAndMarksPartial() {
        FredAdapter adapter;
        const auto response = responseFrom(R"({
            "count":2,"offset":0,"limit":100000,
            "observations":[
                {"date":"2020-01-01","value":"1"},
                {"date":"not-a-date","value":"2"}
            ]
        })");
        const auto result = adapter.normalize(response, fredContext());
        QVERIFY2(result.errors.isEmpty(), qPrintable(result.errors.join("; ")));
        QVERIFY(result.isValid());
        QCOMPARE(result.dataset->observations.size(), qsizetype(1));
        QCOMPARE(result.dataset->quality.status, oqt::data::DataStatus::Partial);
        QVERIFY(result.dataset->quality.isPartial);
    }

    void redactsApiKeyFromRawRequestMetadata() {
        FredAdapter adapter;
        auto context = fredContext();
        context.redactedParameters.insert(QStringLiteral("api_key"),
                                          QStringLiteral("sensitive-test-key"));
        const auto response = responseFrom(R"({"observations":[],"count":0,"offset":0})");
        const auto result = adapter.normalize(response, context);
        QVERIFY(result.rawResult.redactedRequestArguments.join(";")
                    .contains(QStringLiteral("[REDACTED]")));
        QVERIFY(!result.rawResult.redactedRequestArguments.join(";")
                     .contains(QStringLiteral("sensitive-test-key")));
    }

    void malformedOnlyRowsRemainPartialNotEmpty() {
        FredAdapter adapter;
        const auto response = responseFrom(R"({
            "count":1,"offset":0,"limit":100000,
            "observations":[{"date":"invalid-date","value":"12"}]
        })");
        const auto result = adapter.normalize(response, fredContext());
        QVERIFY2(result.errors.isEmpty(), qPrintable(result.errors.join("; ")));
        QVERIFY(result.isValid());
        QCOMPARE(result.dataset->quality.status, oqt::data::DataStatus::Partial);
        QVERIFY(result.dataset->quality.isPartial);
        QVERIFY(result.dataset->observations.isEmpty());
    }

    void rejectsFredApiErrorPayload() {
        FredAdapter adapter;
        const auto response = responseFrom(R"({
            "error_code":400,
            "error_message":"Bad Request. The value for variable api_key is not registered."
        })");
        const auto result = adapter.normalize(response, fredContext());
        QVERIFY(!result.errors.isEmpty());
        QCOMPARE(result.responseStatus, ResponseStatus::Failed);
        QVERIFY(!result.dataset.has_value());
    }

    void rejectsWrongProviderAndMissingSeries() {
        FredAdapter adapter;
        auto context = fredContext();
        context.providerId = QStringLiteral("ecb");
        auto result = adapter.normalize(responseFrom(R"({"observations":[]})"), context);
        QVERIFY(!result.errors.isEmpty());

        context = fredContext();
        context.redactedParameters.remove(QStringLiteral("series_id"));
        result = adapter.normalize(responseFrom(R"({"observations":[]})"), context);
        QVERIFY(!result.errors.isEmpty());
    }
};

QTEST_GUILESS_MAIN(FredAdapterTest)

#include "tst_fred_adapter.moc"
