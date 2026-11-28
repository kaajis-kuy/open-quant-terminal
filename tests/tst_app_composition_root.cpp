#include "app/AppCompositionRoot.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QtTest>

class AppCompositionRootTest final : public QObject {
    Q_OBJECT

private slots:
    void marketExplorerQueryBoundsDisplayRows() {
        oqt::application::MarketExplorerQuery query(
            [](const QString& provider, const QString& instrument, const QString& interval,
               qsizetype pageSize, qint64 offset, QString*)
                -> std::optional<oqt::application::QueryPage<oqt::data::CanonicalMarketSeries>> {
                oqt::data::CanonicalMarketSeries series;
                series.providerId = provider;
                series.instrumentId = instrument;
                series.interval = interval;
                for (int i = 0; i < 3; ++i) {
                    oqt::data::CanonicalMarketBar bar;
                    bar.timestamp = QDateTime::fromSecsSinceEpoch(i + 1, QTimeZone::UTC);
                    bar.open = 1.0; bar.high = 2.0; bar.low = 0.5; bar.close = 1.5;
                    series.bars.append(bar);
                }
                series.bars.remove(0, static_cast<qsizetype>(offset));
                if (series.bars.size() > pageSize) series.bars.removeLast();
                return oqt::application::QueryPage<oqt::data::CanonicalMarketSeries>{
                    std::move(series), offset == 0, offset, pageSize};
            });
        QString error;
        const auto result = query.execute("yahoo_finance", "AAPL", "1d", &error, 2);
        QVERIFY2(result.has_value(), qPrintable(error));
        QCOMPARE(result->series.bars.size(), 2);
        QVERIFY(result->displayTruncated);
        QCOMPARE(result->series.bars.first().timestamp.toSecsSinceEpoch(), qint64(1));
        QVERIFY(result->hasMore);
        QCOMPARE(result->nextOffset, qint64(2));
        QCOMPARE(result->pageSize, qsizetype(2));
    }

    void economicExplorerQueryValidatesRequiredIdentifiers() {
        oqt::application::EconomicDataExplorerQuery query(
            [](const QString&, const QString&, qsizetype, qint64, QString*)
                -> std::optional<oqt::application::QueryPage<oqt::data::CanonicalDataset>> {
                return oqt::application::QueryPage<oqt::data::CanonicalDataset>{};
            });
        QString error;
        QVERIFY(!query.execute("fred", "", &error).has_value());
        QVERIFY(error.contains("dataset"));
    }

    void initializesSeparatePersistentDatabases() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        oqt::app::AppCompositionRoot composition;
        oqt::app::AppCompositionConfig config;
        config.appDataDirectory = directory.path();
        QString error;

        QVERIFY2(composition.initialize(config, &error), qPrintable(error));
        QVERIFY(composition.marketStatus().available);
        QVERIFY(composition.economicStatus().available);
        QVERIFY(QFile::exists(composition.marketDatabasePath()));
        QVERIFY(QFile::exists(composition.economicDatabasePath()));
        QVERIFY(composition.marketDatabasePath() != composition.economicDatabasePath());
        QVERIFY(composition.marketRefreshService() != nullptr);
        QVERIFY(composition.economicRefreshService() != nullptr);
        QVERIFY(composition.marketExplorerQuery() != nullptr);
        QVERIFY(composition.economicDataExplorerQuery() != nullptr);
        QVERIFY(composition.marketInstrumentCatalogQuery() != nullptr);
        QVERIFY(composition.economicDatasetCatalogQuery() != nullptr);
        const auto marketCatalog = composition.marketInstrumentCatalogQuery()->execute({}, 10, 0, &error);
        QVERIFY2(marketCatalog.has_value(), qPrintable(error));
        QVERIFY(marketCatalog->entries.isEmpty());
        const auto economicCatalog = composition.economicDatasetCatalogQuery()->execute({}, 10, 0, &error);
        QVERIFY2(economicCatalog.has_value(), qPrintable(error));
        QVERIFY(economicCatalog->entries.isEmpty());
    }

    void economicDatabaseFailureDoesNotDisableMarketModule() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString blockedPath = QDir(directory.path()).filePath("economic.sqlite");
        QVERIFY(QDir().mkpath(blockedPath)); // SQLite cannot open a directory as a database file.

        oqt::app::AppCompositionConfig config;
        config.appDataDirectory = directory.path();
        config.marketDatabasePath = QDir(directory.path()).filePath("market.sqlite");
        config.economicDatabasePath = blockedPath;

        oqt::app::AppCompositionRoot composition;
        QString error;
        QVERIFY(composition.initialize(config, &error));
        QVERIFY(composition.marketStatus().available);
        QVERIFY(!composition.economicStatus().available);
        QVERIFY(!composition.economicStatus().errorMessage.isEmpty());
        QVERIFY(composition.marketRefreshService() != nullptr);
        QVERIFY(composition.economicRefreshService() == nullptr);
        QVERIFY(composition.economicDataExplorerQuery() == nullptr);\n        QVERIFY(composition.economicDatasetCatalogQuery() == nullptr);
    }

    void marketDatabaseFailureDoesNotDisableEconomicModule() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString blockedPath = QDir(directory.path()).filePath("market.sqlite");
        QVERIFY(QDir().mkpath(blockedPath));

        oqt::app::AppCompositionConfig config;
        config.appDataDirectory = directory.path();
        config.marketDatabasePath = blockedPath;
        config.economicDatabasePath = QDir(directory.path()).filePath("economic.sqlite");

        oqt::app::AppCompositionRoot composition;
        QString error;
        QVERIFY(composition.initialize(config, &error));
        QVERIFY(!composition.marketStatus().available);
        QVERIFY(composition.economicStatus().available);
        QVERIFY(composition.marketRefreshService() == nullptr);
        QVERIFY(composition.marketExplorerQuery() == nullptr);
        QVERIFY(composition.marketInstrumentCatalogQuery() == nullptr);
        QVERIFY(composition.economicRefreshService() != nullptr);
    }

    void rejectsDatabasePathCollision() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString sharedPath = QDir(directory.path()).filePath("shared.sqlite");

        oqt::app::AppCompositionConfig config;
        config.appDataDirectory = directory.path();
        config.marketDatabasePath = sharedPath;
        config.economicDatabasePath = sharedPath;

        oqt::app::AppCompositionRoot composition;
        QString error;
        QVERIFY(!composition.initialize(config, &error));
        QVERIFY(error.contains("different files"));
        QVERIFY(!composition.marketStatus().available);
        QVERIFY(!composition.economicStatus().available);
    }

    void missingFredKeyIsReportedWithoutStartingRequest() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        oqt::app::AppCompositionConfig config;
        config.appDataDirectory = directory.path();
        oqt::app::AppCompositionRoot composition;
        QString error;
        QVERIFY(composition.initialize(config, &error));
        QVERIFY(composition.economicStatus().available);

        qunsetenv("OQT_FRED_API_KEY");
        oqt::providers::FredObservationRequest request;
        request.seriesId = QStringLiteral("GDP");
        QVERIFY(!composition.refreshEconomicData(request, QStringLiteral("fred.gdp"),
                                                 QStringLiteral("no-key"), &error));
        QVERIFY(error.contains("OQT_FRED_API_KEY"));
    }
};

QTEST_GUILESS_MAIN(AppCompositionRootTest)

#include "tst_app_composition_root.moc"
