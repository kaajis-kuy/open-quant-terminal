#include "core/data/MarketData.h"

#include <QtTest>

#include <limits>

using namespace oqt::data;

namespace {

CanonicalMarketBar validBar(const QDateTime& timestamp) {
    CanonicalMarketBar bar;
    bar.timestamp = timestamp;
    bar.open = 100.0;
    bar.high = 110.0;
    bar.low = 95.0;
    bar.close = 105.0;
    bar.adjustedClose = 104.5;
    bar.volume = 1200.0;
    return bar;
}

CanonicalMarketSeries validSeries() {
    CanonicalMarketSeries series;
    series.providerId = QStringLiteral("yahoo");
    series.instrumentId = QStringLiteral("equity.US.AAPL");
    series.providerSymbol = QStringLiteral("AAPL");
    series.interval = QStringLiteral("1d");
    series.currency = QStringLiteral("USD");
    series.bars = {
        validBar(QDateTime::fromString(QStringLiteral("2026-01-02T00:00:00Z"), Qt::ISODate)),
        validBar(QDateTime::fromString(QStringLiteral("2026-01-05T00:00:00Z"), Qt::ISODate))
    };
    series.provenance.sourceUrl = QStringLiteral("https://query1.finance.yahoo.com/v8/finance/chart/AAPL");
    series.provenance.attributionId = QStringLiteral("Yahoo Finance");
    series.provenance.retrievedAt = QDateTime::fromString(
        QStringLiteral("2026-01-05T18:00:00Z"), Qt::ISODate);
    series.quality.status = DataStatus::Ok;
    return series;
}

} // namespace

class MarketDataTest final : public QObject {
    Q_OBJECT

private slots:
    void acceptsValidOhlcvSeries() {
        const CanonicalMarketSeries series = validSeries();
        QVERIFY2(series.validate().isEmpty(), qPrintable(series.validate().join("; ")));
        QCOMPARE(series.bars.size(), qsizetype(2));
    }

    void rejectsMissingIdentityAndInterval() {
        auto series = validSeries();
        series.instrumentId.clear();
        series.providerSymbol.clear();
        series.interval.clear();
        const QStringList errors = series.validate();
        QVERIFY(errors.contains(QStringLiteral("instrumentId is required")));
        QVERIFY(errors.contains(QStringLiteral("providerSymbol is required")));
        QVERIFY(errors.contains(QStringLiteral("interval is required")));
    }

    void rejectsInvalidOhlcRelationships() {
        auto bar = validBar(QDateTime::fromString(
            QStringLiteral("2026-01-02T00:00:00Z"), Qt::ISODate));
        bar.high = 99.0;
        bar.low = 101.0;
        const QStringList errors = bar.validate();
        QVERIFY(errors.contains(QStringLiteral("high cannot be lower than low")));
        QVERIFY(errors.contains(QStringLiteral("high must be greater than or equal to open and close")));
        QVERIFY(errors.contains(QStringLiteral("low must be less than or equal to open and close")));
    }

    void rejectsNonFiniteValuesAndNegativeVolume() {
        auto bar = validBar(QDateTime::fromString(
            QStringLiteral("2026-01-02T00:00:00Z"), Qt::ISODate));
        bar.close = std::numeric_limits<double>::infinity();
        bar.volume = -1.0;
        const QStringList errors = bar.validate();
        QVERIFY(errors.contains(QStringLiteral("OHLC values must be finite")));
        QVERIFY(errors.contains(QStringLiteral("volume must be finite and non-negative when present")));
    }

    void rejectsDuplicateOrDescendingTimestamps() {
        auto series = validSeries();
        series.bars[1].timestamp = series.bars[0].timestamp;
        QVERIFY(series.validate().contains(
            QStringLiteral("bars must be ordered by strictly increasing timestamp")));
    }

    void permitsMissingVolumeAndAdjustedClose() {
        auto series = validSeries();
        series.bars[0].volume.reset();
        series.bars[0].adjustedClose.reset();
        QVERIFY2(series.validate().isEmpty(), qPrintable(series.validate().join("; ")));
    }

    void requiresEmptyStatusForEmptySeries() {
        auto series = validSeries();
        series.bars.clear();
        QVERIFY(series.validate().contains(
            QStringLiteral("an empty market series must have Empty or Partial quality status")));
        series.quality.status = DataStatus::Empty;
        QVERIFY2(series.validate().isEmpty(), qPrintable(series.validate().join("; ")));
    }
};

QTEST_GUILESS_MAIN(MarketDataTest)

#include "tst_market_data.moc"
