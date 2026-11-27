#include "core/services/MarketDataIngestionService.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace oqt;

namespace {

data::CanonicalMarketSeries makeSeries(double firstClose = 11.0,
                                        double secondClose = 12.0) {
    data::CanonicalMarketSeries series;
    series.providerId = QStringLiteral("test_provider");
    series.instrumentId = QStringLiteral("asset.test");
    series.providerSymbol = QStringLiteral("TEST");
    series.interval = QStringLiteral("1d");
    series.currency = QStringLiteral("USD");
    series.provenance.attributionId = QStringLiteral("test");
    series.provenance.retrievedAt = QDateTime::fromString(
        QStringLiteral("2026-01-01T00:00:00Z"), Qt::ISODate);

    data::CanonicalMarketBar first;
    first.timestamp = QDateTime::fromString(
        QStringLiteral("2026-01-01T00:00:00Z"), Qt::ISODate);
    first.open = 10.0;
    first.high = 12.0;
    first.low = 9.0;
    first.close = firstClose;
    first.high = qMax(first.high, firstClose);
    first.volume = 100.0;

    data::CanonicalMarketBar second = first;
    second.timestamp = QDateTime::fromString(
        QStringLiteral("2026-01-02T00:00:00Z"), Qt::ISODate);
    second.close = secondClose;
    second.high = qMax(second.high, secondClose);
    second.volume = 120.0;

    series.bars = {first, second};
    return series;
}

providers::AdapterResult makeResult(data::CanonicalMarketSeries series) {
    providers::AdapterResult result;
    result.responseStatus = providers::ResponseStatus::Ok;
    result.marketSeries = std::move(series);
    result.rawResult.providerId = QStringLiteral("test_provider");
    result.rawResult.operationId = QStringLiteral("historical_ohlcv");
    result.rawResult.requestId = QStringLiteral("test-request");
    result.rawResult.responseSchemaVersion = QStringLiteral("1.0");
    result.rawResult.fetchedAt = QDateTime::currentDateTimeUtc();
    result.rawResult.rawPayload = QJsonObject{{QStringLiteral("ok"), true}};
    return result;
}

} // namespace

class MarketDataIngestionServiceTest final : public QObject {
    Q_OBJECT

private slots:
    void persistsValidMarketSeriesWithReplaceMode() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        storage::SqliteMarketDataStore store(
            directory.filePath(QStringLiteral("market.sqlite")));
        QVERIFY(store.open());

        services::MarketDataIngestionService service(store);
        QString error;
        QVERIFY2(service.persist(makeResult(makeSeries()),
                                 services::MarketDataWriteMode::Replace, &error),
                 qPrintable(error));

        const auto loaded = store.loadSeries(
            QStringLiteral("test_provider"), QStringLiteral("asset.test"),
            QStringLiteral("1d"), &error);
        QVERIFY2(loaded.has_value(), qPrintable(error));
        QCOMPARE(loaded->bars.size(), qsizetype(2));
        QCOMPARE(loaded->bars.at(0).close, 11.0);
    }

    void upsertModeMergesIncrementalBars() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        storage::SqliteMarketDataStore store(
            directory.filePath(QStringLiteral("market.sqlite")));
        QVERIFY(store.open());

        QString error;
        QVERIFY(store.saveSeries(makeSeries(), &error));
        auto update = makeSeries(15.0, 16.0);
        data::CanonicalMarketBar newest = update.bars.last();
        newest.timestamp = QDateTime::fromString(
            QStringLiteral("2026-01-03T00:00:00Z"), Qt::ISODate);
        newest.close = 17.0;
        newest.high = qMax(newest.high, newest.close);
        update.bars = {update.bars.first(), newest};

        services::MarketDataIngestionService service(store);
        QVERIFY2(service.persist(makeResult(update),
                                 services::MarketDataWriteMode::Upsert, &error),
                 qPrintable(error));

        const auto loaded = store.loadSeries(
            QStringLiteral("test_provider"), QStringLiteral("asset.test"),
            QStringLiteral("1d"), &error);
        QVERIFY2(loaded.has_value(), qPrintable(error));
        QCOMPARE(loaded->bars.size(), qsizetype(3));
        QCOMPARE(loaded->bars.at(0).close, 15.0);
        QCOMPARE(loaded->bars.at(1).close, 12.0);
        QCOMPARE(loaded->bars.at(2).close, 17.0);
    }

    void rejectsInvalidAdapterResult() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        storage::SqliteMarketDataStore store(
            directory.filePath(QStringLiteral("market.sqlite")));
        QVERIFY(store.open());

        providers::AdapterResult invalid;
        services::MarketDataIngestionService service(store);
        QString error;
        QVERIFY(!service.persist(invalid, services::MarketDataWriteMode::Replace, &error));
        QVERIFY(error.contains(QStringLiteral("invalid")));
    }

    void rejectsProviderIdentityMismatchWithoutWriting() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        storage::SqliteMarketDataStore store(
            directory.filePath(QStringLiteral("market.sqlite")));
        QVERIFY(store.open());

        auto result = makeResult(makeSeries());
        result.rawResult.providerId = QStringLiteral("different_provider");

        services::MarketDataIngestionService service(store);
        QString error;
        QVERIFY(!service.persist(result, services::MarketDataWriteMode::Replace, &error));
        QVERIFY(error.contains(QStringLiteral("invalid")));

        const auto loaded = store.loadSeries(
            QStringLiteral("test_provider"), QStringLiteral("asset.test"),
            QStringLiteral("1d"), &error);
        QVERIFY2(!loaded.has_value(), qPrintable(error));
    }

    void rejectsScalarDatasetResult() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        storage::SqliteMarketDataStore store(
            directory.filePath(QStringLiteral("market.sqlite")));
        QVERIFY(store.open());

        auto result = makeResult(makeSeries());
        result.marketSeries.reset();
        data::CanonicalDataset dataset;
        dataset.providerId = QStringLiteral("test_provider");
        dataset.datasetId = QStringLiteral("scalar.dataset");
        dataset.provenance.retrievedAt = QDateTime::currentDateTimeUtc();
        result.dataset = dataset;

        services::MarketDataIngestionService service(store);
        QString error;
        QVERIFY(!service.persist(result, services::MarketDataWriteMode::Replace, &error));
        QVERIFY(error.contains(QStringLiteral("market series")));
    }

    void rejectsClosedStore() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        storage::SqliteMarketDataStore store(
            directory.filePath(QStringLiteral("market.sqlite")));
        services::MarketDataIngestionService service(store);

        QString error;
        QVERIFY(!service.persist(makeResult(makeSeries()),
                                 services::MarketDataWriteMode::Replace, &error));
        QVERIFY(error.contains(QStringLiteral("not open")));
    }
};

QTEST_GUILESS_MAIN(MarketDataIngestionServiceTest)

#include "tst_market_data_ingestion_service.moc"
