#include "app/AppCompositionRoot.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class AppCompositionRootTest final : public QObject {
    Q_OBJECT

private slots:
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
        QVERIFY(composition.economicDataExplorerQuery() == nullptr);
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
