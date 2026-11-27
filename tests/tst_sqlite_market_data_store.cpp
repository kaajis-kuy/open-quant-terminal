#include "core/storage/SqliteMarketDataStore.h"

#include <QtTest>

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
