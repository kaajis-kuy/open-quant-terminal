#include "core/providers/YahooFinanceAdapter.h"

#include <QtTest>

using namespace oqt::providers;

namespace {

ProviderRequestContext yahooContext() {
    ProviderRequestContext context;
    context.providerId = QStringLiteral("yahoo_finance");
    context.operationId = QStringLiteral("historical_ohlcv");
    context.datasetId = QStringLiteral("instrument.aapl");
    context.requestId = QStringLiteral("yahoo-fixture-001");
    context.requestedAt = QDateTime::fromString(
        QStringLiteral("2026-11-27T12:00:00Z"), Qt::ISODate);
    context.redactedParameters.insert(QStringLiteral("symbol"), QStringLiteral("AAPL"));
    context.redactedParameters.insert(QStringLiteral("interval"), QStringLiteral("1d"));
    context.redactedParameters.insert(QStringLiteral("range"), QStringLiteral("1mo"));
    return context;
}

ProviderResponse responseFrom(const QByteArray& json) {
    return ProviderResponse::decode(json);
}

QByteArray validChartFixture() {
    return R"({
        "chart": {
            "result": [{
                "meta": {"symbol":"AAPL","currency":"USD","exchangeTimezoneName":"America/New_York"},
                "timestamp": [1795737600,1795824000],
                "indicators": {
                    "quote": [{
                        "open": [270.0,272.0],
                        "high": [275.0,276.0],
                        "low": [268.0,270.0],
                        "close": [273.0,274.0],
                        "volume": [1000000,1200000]
                    }],
                    "adjclose": [{"adjclose":[272.5,273.5]}]
                }
            }],
            "error": null
        }
    })";
}

} // namespace

class YahooFinanceAdapterTest final : public QObject {
    Q_OBJECT

private slots:
    void providerIdentityAndOperations() {
        YahooFinanceAdapter adapter;
        QCOMPARE(adapter.providerId(), QStringLiteral("yahoo_finance"));
        QVERIFY(adapter.supportsOperation(QStringLiteral("historical_ohlcv")));
        QVERIFY(!adapter.supportsOperation(QStringLiteral("quote")));
    }

    void normalizesHistoricalOhlcv() {
        YahooFinanceAdapter adapter;
        const auto result = adapter.normalize(responseFrom(validChartFixture()), yahooContext());
        QVERIFY2(result.errors.isEmpty(), qPrintable(result.errors.join("; ")));
        QVERIFY(result.isValid());
        QVERIFY(result.marketSeries.has_value());
        QVERIFY(!result.dataset.has_value());
        const auto& series = *result.marketSeries;
        QCOMPARE(series.providerId, QStringLiteral("yahoo_finance"));
        QCOMPARE(series.instrumentId, QStringLiteral("instrument.aapl"));
        QCOMPARE(series.providerSymbol, QStringLiteral("AAPL"));
        QCOMPARE(series.interval, QStringLiteral("1d"));
        QCOMPARE(series.currency, QStringLiteral("USD"));
        QCOMPARE(series.bars.size(), qsizetype(2));
        QCOMPARE(series.bars.at(0).timestamp.timeSpec(), Qt::UTC);
        QCOMPARE(series.bars.at(0).open, 270.0);
        QCOMPARE(series.bars.at(0).high, 275.0);
        QCOMPARE(series.bars.at(0).low, 268.0);
        QCOMPARE(series.bars.at(0).close, 273.0);
        QCOMPARE(*series.bars.at(0).adjustedClose, 272.5);
        QCOMPARE(*series.bars.at(0).volume, 1000000.0);
        QCOMPARE(series.quality.status, oqt::data::DataStatus::Ok);
    }

    void skipsNullCandlesAndMarksSeriesPartial() {
        YahooFinanceAdapter adapter;
        const auto response = responseFrom(R"({
            "chart":{"result":[{
                "meta":{"symbol":"AAPL","currency":"USD"},
                "timestamp":[1795737600,1795824000],
                "indicators":{"quote":[{
                    "open":[270.0,null],"high":[275.0,null],
                    "low":[268.0,null],"close":[273.0,null],
                    "volume":[1000000,null]
                }]}
            }],"error":null}
        })");
        const auto result = adapter.normalize(response, yahooContext());
        QVERIFY2(result.errors.isEmpty(), qPrintable(result.errors.join("; ")));
        QVERIFY(result.isValid());
        QCOMPARE(result.marketSeries->bars.size(), qsizetype(1));
        QCOMPARE(result.marketSeries->quality.status, oqt::data::DataStatus::Partial);
        QVERIFY(result.marketSeries->quality.isPartial);
    }

    void missingVolumeAndAdjustedCloseRemainOptional() {
        YahooFinanceAdapter adapter;
        const auto response = responseFrom(R"({
            "chart":{"result":[{
                "meta":{"symbol":"AAPL"},
                "timestamp":[1795737600],
                "indicators":{"quote":[{
                    "open":[10.0],"high":[12.0],"low":[9.0],"close":[11.0]
                }]}
            }],"error":null}
        })");
        const auto result = adapter.normalize(response, yahooContext());
        QVERIFY2(result.errors.isEmpty(), qPrintable(result.errors.join("; ")));
        QVERIFY(result.isValid());
        QVERIFY(!result.marketSeries->bars.first().volume.has_value());
        QVERIFY(!result.marketSeries->bars.first().adjustedClose.has_value());
    }

    void rejectsMismatchedSymbolAndMalformedPayload() {
        YahooFinanceAdapter adapter;
        auto response = responseFrom(validChartFixture());
        auto context = yahooContext();
        context.redactedParameters.insert(QStringLiteral("symbol"), QStringLiteral("MSFT"));
        QVERIFY(!adapter.normalize(response, context).errors.isEmpty());

        response = responseFrom(R"({"chart":{"error":{"code":"Not Found","description":"No data found"}}})");
        const auto failed = adapter.normalize(response, yahooContext());
        QVERIFY(!failed.errors.isEmpty());
        QCOMPARE(failed.responseStatus, ResponseStatus::Failed);
    }

    void marksTruncatedResponsePartial() {
        YahooFinanceAdapter adapter;
        auto response = responseFrom(validChartFixture());
        response.status = ResponseStatus::Partial;
        response.truncated = true;
        response.nextPageCursor = QStringLiteral("period2=1796000000");
        const auto result = adapter.normalize(response, yahooContext());
        QVERIFY2(result.errors.isEmpty(), qPrintable(result.errors.join("; ")));
        QVERIFY(result.isValid());
        QCOMPARE(result.marketSeries->quality.status, oqt::data::DataStatus::Partial);
        QVERIFY(result.marketSeries->quality.isTruncated);
        QCOMPARE(result.marketSeries->quality.nextPageCursor, QStringLiteral("period2=1796000000"));
    }

    void rejectsFractionalAndOutOfRangeTimestamps() {
        YahooFinanceAdapter adapter;
        const auto fractional = responseFrom(R"({
            "chart":{"result":[{
                "meta":{"symbol":"AAPL"},
                "timestamp":[1795737600.5],
                "indicators":{"quote":[{
                    "open":[10.0],"high":[12.0],"low":[9.0],"close":[11.0]
                }]}
            }],"error":null}
        })");
        const auto fractionalResult = adapter.normalize(fractional, yahooContext());
        QVERIFY(fractionalResult.isValid());
        QVERIFY(fractionalResult.marketSeries->bars.isEmpty());
        QCOMPARE(fractionalResult.marketSeries->quality.status, oqt::data::DataStatus::Partial);

        auto context = yahooContext();
        const auto outOfRange = responseFrom(R"({
            "chart":{"result":[{
                "meta":{"symbol":"AAPL"},
                "timestamp":[1e30],
                "indicators":{"quote":[{
                    "open":[10.0],"high":[12.0],"low":[9.0],"close":[11.0]
                }]}
            }],"error":null}
        })");
        const auto outOfRangeResult = adapter.normalize(outOfRange, context);
        QVERIFY(outOfRangeResult.isValid());
        QVERIFY(outOfRangeResult.marketSeries->bars.isEmpty());
        QCOMPARE(outOfRangeResult.marketSeries->quality.status, oqt::data::DataStatus::Partial);
    }

    void marksShortOptionalIndicatorArraysPartial() {
        YahooFinanceAdapter adapter;
        const auto response = responseFrom(R"({
            "chart":{"result":[{
                "meta":{"symbol":"AAPL"},
                "timestamp":[1795737600,1795824000],
                "indicators":{"quote":[{
                    "open":[10.0,11.0],"high":[12.0,13.0],
                    "low":[9.0,10.0],"close":[11.0,12.0],
                    "volume":[100]
                }],"adjclose":[{"adjclose":[10.5]}]}
            }],"error":null}
        })");
        const auto result = adapter.normalize(response, yahooContext());
        QVERIFY2(result.errors.isEmpty(), qPrintable(result.errors.join("; ")));
        QVERIFY(result.isValid());
        QCOMPARE(result.marketSeries->bars.size(), qsizetype(2));
        QCOMPARE(result.marketSeries->quality.status, oqt::data::DataStatus::Partial);
        QVERIFY(result.marketSeries->quality.isPartial);
        QCOMPARE(result.marketSeries->quality.missingValueCounts.value(QStringLiteral("volume")), qsizetype(1));
    }

    void rejectsUnorderedProviderTimestampsAsPartial() {
        YahooFinanceAdapter adapter;
        const auto response = responseFrom(R"({
            "chart":{"result":[{
                "meta":{"symbol":"AAPL"},
                "timestamp":[1795824000,1795737600],
                "indicators":{"quote":[{
                    "open":[10.0,11.0],"high":[12.0,13.0],
                    "low":[9.0,10.0],"close":[11.0,12.0]
                }]}
            }],"error":null}
        })");
        const auto result = adapter.normalize(response, yahooContext());
        QVERIFY(!result.errors.isEmpty());
        QVERIFY(result.marketSeries.has_value());
    }

    void redactsSensitiveRequestArguments() {
        YahooFinanceAdapter adapter;
        auto context = yahooContext();
        context.redactedParameters.insert(QStringLiteral("api_token"),
                                          QStringLiteral("sensitive-fixture-token"));
        const auto result = adapter.normalize(responseFrom(validChartFixture()), context);
        QVERIFY(result.rawResult.redactedRequestArguments.join(";")
                    .contains(QStringLiteral("[REDACTED]")));
        QVERIFY(!result.rawResult.redactedRequestArguments.join(";")
                     .contains(QStringLiteral("sensitive-fixture-token")));
    }
};

QTEST_GUILESS_MAIN(YahooFinanceAdapterTest)

#include "tst_yahoo_finance_adapter.moc"
