#include "core/providers/YahooRequestBuilder.h"

#include <QUrlQuery>
#include <QtTest>

using namespace oqt::providers;

class YahooRequestBuilderTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsDefaultChartRequest() {
        YahooChartRequest input;
        input.symbol = QStringLiteral("AAPL");
        const auto result = YahooRequestBuilder::chart(input);
        QVERIFY2(result.isValid(), qPrintable(result.error));
        QCOMPARE(result.url.scheme(), QStringLiteral("https"));
        QCOMPARE(result.url.host(), QStringLiteral("query1.finance.yahoo.com"));
        QCOMPARE(result.url.path(), QStringLiteral("/v8/finance/chart/AAPL"));
        const QUrlQuery query(result.url);
        QCOMPARE(query.queryItemValue(QStringLiteral("interval")), QStringLiteral("1d"));
        QCOMPARE(query.queryItemValue(QStringLiteral("range")), QStringLiteral("1y"));
        QCOMPARE(query.queryItemValue(QStringLiteral("includeAdjustedClose")), QStringLiteral("true"));
        QCOMPARE(query.queryItemValue(QStringLiteral("events")), QStringLiteral("div,splits"));
    }

    void buildsExplicitUnixPeriodRange() {
        YahooChartRequest input;
        input.symbol = QStringLiteral("BTC-USD");
        input.interval = QStringLiteral("1h");
        input.period1 = 1795737600;
        input.period2 = 1796083200;
        const auto result = YahooRequestBuilder::chart(input);
        QVERIFY2(result.isValid(), qPrintable(result.error));
        const QUrlQuery query(result.url);
        QCOMPARE(query.queryItemValue(QStringLiteral("period1")), QStringLiteral("1795737600"));
        QCOMPARE(query.queryItemValue(QStringLiteral("period2")), QStringLiteral("1796083200"));
        QVERIFY(!query.hasQueryItem(QStringLiteral("range")));
    }

    void supportsEndpointOverrideAndEncodedSymbol() {
        YahooChartRequest input;
        input.symbol = QStringLiteral("^GSPC");
        const auto result = YahooRequestBuilder::chart(
            input, QUrl(QStringLiteral("http://127.0.0.1:8080/mock")));
        QVERIFY(result.isValid());
        QCOMPARE(result.url.scheme(), QStringLiteral("http"));
        QCOMPARE(result.url.host(), QStringLiteral("127.0.0.1"));
        QCOMPARE(result.url.path(), QStringLiteral("/mock/v8/finance/chart/%5EGSPC"));
    }

    void omitsOptionalEventsAndAdjustedClose() {
        YahooChartRequest input;
        input.symbol = QStringLiteral("MSFT");
        input.includeEvents = false;
        input.includeAdjustedClose = false;
        const auto result = YahooRequestBuilder::chart(input);
        QVERIFY(result.isValid());
        const QUrlQuery query(result.url);
        QVERIFY(!query.hasQueryItem(QStringLiteral("events")));
        QCOMPARE(query.queryItemValue(QStringLiteral("includeAdjustedClose")),
                 QStringLiteral("false"));
    }

    void rejectsMissingOrUnsafeSymbol() {
        YahooChartRequest input;
        QVERIFY(!YahooRequestBuilder::chart(input).isValid());
        input.symbol = QStringLiteral("../AAPL");
        QVERIFY(!YahooRequestBuilder::chart(input).isValid());
        input.symbol = QStringLiteral("AAPL?range=max");
        QVERIFY(!YahooRequestBuilder::chart(input).isValid());
    }

    void rejectsUnsupportedIntervalsAndRanges() {
        YahooChartRequest input;
        input.symbol = QStringLiteral("AAPL");
        input.interval = QStringLiteral("7m");
        QVERIFY(!YahooRequestBuilder::chart(input).isValid());
        input.interval = QStringLiteral("1d");
        input.range = QStringLiteral("20y");
        QVERIFY(!YahooRequestBuilder::chart(input).isValid());
    }

    void rejectsInvalidPeriodPairs() {
        YahooChartRequest input;
        input.symbol = QStringLiteral("AAPL");
        input.period1 = 100;
        QVERIFY(!YahooRequestBuilder::chart(input).isValid());
        input.period2 = 100;
        QVERIFY(!YahooRequestBuilder::chart(input).isValid());
        input.period1 = 200;
        input.period2 = 100;
        QVERIFY(!YahooRequestBuilder::chart(input).isValid());
    }

    void rejectsUnsupportedEndpointScheme() {
        YahooChartRequest input;
        input.symbol = QStringLiteral("AAPL");
        const auto result = YahooRequestBuilder::chart(
            input, QUrl(QStringLiteral("file:///tmp/mock")));
        QVERIFY(!result.isValid());
    }
};

QTEST_GUILESS_MAIN(YahooRequestBuilderTest)

#include "tst_yahoo_request_builder.moc"
