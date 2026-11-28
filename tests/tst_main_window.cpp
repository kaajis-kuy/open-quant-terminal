#include "app/MainWindow.h"
#include "application/CatalogQueries.h"
#include "application/DataExplorerQueries.h"
#include "application/RefreshTaskController.h"

#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QtTest>

class FakeRefreshTaskController final : public oqt::application::RefreshTaskController {
public:
    bool startMarketRefresh(const QString& symbol, const QString& instrument,
                            const QString& interval, const QString& requestId) override {
        marketSymbol = symbol;
        marketInstrument = instrument;
        marketInterval = interval;
        marketRequestId = requestId;
        return allowStart;
    }
    bool startEconomicRefresh(const QString& dataset, const QString& requestId,
                              QString* errorMessage) override {
        economicDataset = dataset;
        economicRequestId = requestId;
        if (!allowStart && errorMessage) *errorMessage = QStringLiteral("test start failure");
        return allowStart;
    }
    void cancelMarketRefresh() override { ++marketCancelCount; }
    void cancelEconomicRefresh() override { ++economicCancelCount; }

    void finishMarket(bool persisted, const QString& error = {}) {
        oqt::providers::AdapterResult result;
        emit marketRefreshFinished(marketRequestId, persisted, error, result);
    }
    void finishEconomic(bool persisted, const QString& error = {}) {
        oqt::providers::AdapterResult result;
        emit economicRefreshFinished(economicRequestId, persisted, error, result);
    }

    bool allowStart = true;
    QString marketSymbol;
    QString marketInstrument;
    QString marketInterval;
    QString marketRequestId;
    QString economicDataset;
    QString economicRequestId;
    int marketCancelCount = 0;
    int economicCancelCount = 0;
};

class MainWindowTest final : public QObject {
    Q_OBJECT

private slots:
    void window_has_expected_identity() {
        MainWindow window;

        QCOMPARE(window.objectName(), QString("mainWindow"));
        QCOMPARE(window.windowTitle(), QString("Open Quant Terminal"));
    }

    void window_exposes_welcome_content() {
        MainWindow window;

        const auto* title = window.findChild<QLabel*>("welcomeTitle");
        const auto* subtitle = window.findChild<QLabel*>("welcomeSubtitle");

        QVERIFY(title != nullptr);
        QVERIFY(subtitle != nullptr);
        QVERIFY(title->text().contains("Open Quant Terminal"));
        QVERIFY(subtitle->text().contains("Quantitative research terminal"));
    }

    void window_exposes_market_and_economic_explorers() {
        MainWindow window;
        const auto* tabs = window.findChild<QTabWidget*>("workspaceTabs");
        QVERIFY(tabs != nullptr);
        QCOMPARE(tabs->count(), 2);
        QCOMPARE(tabs->tabText(0), QString("Market Explorer"));
        QCOMPARE(tabs->tabText(1), QString("Economic Data Explorer"));
        QVERIFY(window.findChild<QTableWidget*>("marketDataTable") != nullptr);
        QVERIFY(window.findChild<QTableWidget*>("economicDataTable") != nullptr);
    }

    void catalogSelectorsPopulateAndFillExplorerInputs() {
        oqt::application::MarketExplorerQuery marketQuery(
            [](const QString&, const QString&, const QString&, qsizetype, qint64, QString*)
                -> std::optional<oqt::application::QueryPage<oqt::data::CanonicalMarketSeries>> {
                return std::nullopt;
            });
        oqt::application::EconomicDataExplorerQuery economicQuery(
            [](const QString&, const QString&, qsizetype, qint64, QString*)
                -> std::optional<oqt::application::QueryPage<oqt::data::CanonicalDataset>> {
                return std::nullopt;
            });
        oqt::application::MarketInstrumentCatalogQuery marketCatalog(
            [](const QString&, qsizetype pageSize, qint64 offset, QString*)
                -> std::optional<oqt::data::MarketInstrumentCatalogPage> {
                oqt::data::MarketInstrumentCatalogPage page;
                page.pageSize = pageSize;
                page.offset = offset;
                oqt::data::MarketInstrumentCatalogEntry entry;
                entry.providerId = QStringLiteral("yahoo_finance");
                entry.instrumentId = QStringLiteral("equity.aapl");
                entry.providerSymbol = QStringLiteral("AAPL");
                entry.interval = QStringLiteral("1d");
                entry.candleCount = 42;
                page.entries.append(entry);
                return page;
            });
        oqt::application::EconomicDatasetCatalogQuery economicCatalog(
            [](const QString&, qsizetype pageSize, qint64 offset, QString*)
                -> std::optional<oqt::data::EconomicDatasetCatalogPage> {
                oqt::data::EconomicDatasetCatalogPage page;
                page.pageSize = pageSize;
                page.offset = offset;
                oqt::data::EconomicDatasetCatalogEntry entry;
                entry.providerId = QStringLiteral("fred");
                entry.datasetId = QStringLiteral("fred.gdp");
                entry.observationCount = 8;
                page.entries.append(entry);
                return page;
            });

        MainWindow window(&marketQuery, &economicQuery, &marketCatalog, &economicCatalog);
        auto* marketSelector = window.findChild<QComboBox*>("marketCatalogSelector");
        auto* economicSelector = window.findChild<QComboBox*>("economicCatalogSelector");
        QVERIFY(marketSelector != nullptr);
        QVERIFY(economicSelector != nullptr);
        QCOMPARE(marketSelector->count(), 2);
        QCOMPARE(economicSelector->count(), 2);

        QVERIFY(QMetaObject::invokeMethod(marketSelector, "activated",
                                          Qt::DirectConnection, Q_ARG(int, 1)));
        QVERIFY(QMetaObject::invokeMethod(economicSelector, "activated",
                                          Qt::DirectConnection, Q_ARG(int, 1)));
        QCOMPARE(window.findChild<QLineEdit*>("marketProviderInput")->text(),
                 QStringLiteral("yahoo_finance"));
        QCOMPARE(window.findChild<QLineEdit*>("marketInstrumentInput")->text(),
                 QStringLiteral("equity.aapl"));
        QCOMPARE(window.findChild<QLineEdit*>("marketIntervalInput")->text(),
                 QStringLiteral("1d"));
        QCOMPARE(window.findChild<QLineEdit*>("economicProviderInput")->text(),
                 QStringLiteral("fred"));
        QCOMPARE(window.findChild<QLineEdit*>("economicDatasetInput")->text(),
                 QStringLiteral("fred.gdp"));
    }

    void catalogPaginationControlsNavigateAndDisableAtBoundaries() {
        oqt::application::MarketInstrumentCatalogQuery marketCatalog(
            [](const QString&, qsizetype pageSize, qint64 offset, QString*) {
                oqt::data::MarketInstrumentCatalogPage page;
                page.pageSize = pageSize;
                page.offset = offset;
                oqt::data::MarketInstrumentCatalogEntry entry;
                entry.providerId = QStringLiteral("yahoo_finance");
                entry.interval = QStringLiteral("1d");
                entry.providerSymbol = offset == 0 ? QStringLiteral("AAPL") : QStringLiteral("MSFT");
                entry.instrumentId = offset == 0 ? QStringLiteral("equity.aapl")
                                                 : QStringLiteral("equity.msft");
                page.entries.append(entry);
                page.hasMore = offset == 0;
                return std::optional<oqt::data::MarketInstrumentCatalogPage>(page);
            });
        oqt::application::EconomicDatasetCatalogQuery economicCatalog(
            [](const QString&, qsizetype pageSize, qint64 offset, QString*) {
                oqt::data::EconomicDatasetCatalogPage page;
                page.pageSize = pageSize;
                page.offset = offset;
                oqt::data::EconomicDatasetCatalogEntry entry;
                entry.providerId = QStringLiteral("fred");
                entry.datasetId = offset == 0 ? QStringLiteral("fred.gdp")
                                              : QStringLiteral("fred.cpi");
                page.entries.append(entry);
                page.hasMore = offset == 0;
                return std::optional<oqt::data::EconomicDatasetCatalogPage>(page);
            });

        MainWindow window(nullptr, nullptr, &marketCatalog, &economicCatalog);
        auto* marketSelector = window.findChild<QComboBox*>("marketCatalogSelector");
        auto* marketPrevious = window.findChild<QPushButton*>("marketCatalogPreviousButton");
        auto* marketNext = window.findChild<QPushButton*>("marketCatalogNextButton");
        auto* marketPageLabel = window.findChild<QLabel*>("marketCatalogPageLabel");
        auto* economicSelector = window.findChild<QComboBox*>("economicCatalogSelector");
        auto* economicPrevious = window.findChild<QPushButton*>("economicCatalogPreviousButton");
        auto* economicNext = window.findChild<QPushButton*>("economicCatalogNextButton");
        auto* economicPageLabel = window.findChild<QLabel*>("economicCatalogPageLabel");

        QVERIFY(marketSelector && marketPrevious && marketNext && marketPageLabel);
        QVERIFY(economicSelector && economicPrevious && economicNext && economicPageLabel);
        QVERIFY(!marketPrevious->isEnabled());
        QVERIFY(marketNext->isEnabled());
        QVERIFY(!economicPrevious->isEnabled());
        QVERIFY(economicNext->isEnabled());

        marketNext->click();
        economicNext->click();
        QCOMPARE(marketPageLabel->text(), QStringLiteral("Page 2"));
        QCOMPARE(economicPageLabel->text(), QStringLiteral("Page 2"));
        QVERIFY(marketSelector->itemText(1).contains(QStringLiteral("MSFT")));
        QVERIFY(economicSelector->itemText(1).contains(QStringLiteral("fred.cpi")));
        QVERIFY(marketPrevious->isEnabled());
        QVERIFY(!marketNext->isEnabled());
        QVERIFY(economicPrevious->isEnabled());
        QVERIFY(!economicNext->isEnabled());

        marketPrevious->click();
        economicPrevious->click();
        QCOMPARE(marketPageLabel->text(), QStringLiteral("Page 1"));
        QCOMPARE(economicPageLabel->text(), QStringLiteral("Page 1"));
        QVERIFY(marketNext->isEnabled());
        QVERIFY(economicNext->isEnabled());
        QVERIFY(!marketPrevious->isEnabled());
        QVERIFY(!economicPrevious->isEnabled());
    }

    void explorersExposeDedicatedChartViews() {
        MainWindow window;
        auto* marketChart = window.findChild<QWidget*>("marketChart");
        auto* economicChart = window.findChild<QWidget*>("economicChart");
        QVERIFY(marketChart != nullptr);
        QVERIFY(economicChart != nullptr);
        QVERIFY(marketChart->minimumHeight() >= 200);
        QVERIFY(economicChart->minimumHeight() >= 200);
    }

    void refreshTaskControlsExposeIdleStateWithoutProviders() {
        MainWindow window;
        auto* status = window.findChild<QLabel*>("refreshTaskStatus");
        auto* progress = window.findChild<QProgressBar*>("refreshTaskProgress");
        auto* cancel = window.findChild<QPushButton*>("cancelRefreshButton");
        auto* marketRefresh = window.findChild<QPushButton*>("refreshMarketButton");
        auto* economicRefresh = window.findChild<QPushButton*>("refreshEconomicButton");
        QVERIFY(status && progress && cancel && marketRefresh && economicRefresh);
        QCOMPARE(status->text(), QStringLiteral("No refresh task running"));
        QVERIFY(!cancel->isEnabled());
        QVERIFY(!marketRefresh->isEnabled());
        QVERIFY(!economicRefresh->isEnabled());
        QCOMPARE(progress->minimum(), 0);
        QCOMPARE(progress->maximum(), 1);
        QCOMPARE(progress->value(), 0);
    }

    void refreshActionsStartTasksAndExposeCancellation() {
        oqt::application::MarketExplorerQuery marketQuery(
            [](const QString&, const QString&, const QString&, qsizetype, qint64, QString*)
                -> std::optional<oqt::application::QueryPage<oqt::data::CanonicalMarketSeries>> {
                return std::nullopt;
            });
        FakeRefreshTaskController controller;
        MainWindow window(&marketQuery, nullptr, nullptr, nullptr, &controller);
        auto* marketInstrument = window.findChild<QLineEdit*>("marketInstrumentInput");
        auto* marketRefresh = window.findChild<QPushButton*>("refreshMarketButton");
        auto* cancel = window.findChild<QPushButton*>("cancelRefreshButton");
        auto* status = window.findChild<QLabel*>("refreshTaskStatus");
        auto* progress = window.findChild<QProgressBar*>("refreshTaskProgress");
        QVERIFY(marketInstrument && marketRefresh && cancel && status && progress);

        marketInstrument->setText(QStringLiteral("equity.msft"));
        marketRefresh->click();
        QCOMPARE(controller.marketSymbol, QStringLiteral("equity.msft"));
        QCOMPARE(controller.marketInstrument, QStringLiteral("equity.msft"));
        QCOMPARE(controller.marketInterval, QStringLiteral("1d"));
        QVERIFY(!controller.marketRequestId.isEmpty());
        QVERIFY(cancel->isEnabled());
        QCOMPARE(progress->maximum(), 0);
        QCOMPARE(status->text(), QStringLiteral("Refreshing market data…"));

        cancel->click();
        QCOMPARE(controller.marketCancelCount, 1);
        QCOMPARE(status->text(), QStringLiteral("Cancelling market refresh…"));
    }


    void marketRefreshCompletionRestoresIdleUiAndReportsPersistence() {
        oqt::application::MarketExplorerQuery marketQuery(
            [](const QString&, const QString&, const QString&, qsizetype, qint64, QString*)
                -> std::optional<oqt::application::QueryPage<oqt::data::CanonicalMarketSeries>> {
                return std::nullopt;
            });
        FakeRefreshTaskController controller;
        MainWindow window(&marketQuery, nullptr, nullptr, nullptr, &controller);
        auto* refresh = window.findChild<QPushButton*>("refreshMarketButton");
        auto* cancel = window.findChild<QPushButton*>("cancelRefreshButton");
        auto* status = window.findChild<QLabel*>("refreshTaskStatus");
        auto* progress = window.findChild<QProgressBar*>("refreshTaskProgress");
        QVERIFY(refresh && cancel && status && progress);

        refresh->click();
        QVERIFY(cancel->isEnabled());
        controller.finishMarket(true);

        QCOMPARE(status->text(), QStringLiteral("Market refresh completed and persisted"));
        QVERIFY(!cancel->isEnabled());
        QVERIFY(refresh->isEnabled());
        QCOMPARE(progress->maximum(), 1);
        QCOMPARE(progress->value(), 0);
    }

    void marketRefreshFailureShowsActionableErrorAndRestoresControls() {
        oqt::application::MarketExplorerQuery marketQuery(
            [](const QString&, const QString&, const QString&, qsizetype, qint64, QString*)
                -> std::optional<oqt::application::QueryPage<oqt::data::CanonicalMarketSeries>> {
                return std::nullopt;
            });
        FakeRefreshTaskController controller;
        MainWindow window(&marketQuery, nullptr, nullptr, nullptr, &controller);
        auto* refresh = window.findChild<QPushButton*>("refreshMarketButton");
        auto* cancel = window.findChild<QPushButton*>("cancelRefreshButton");
        auto* status = window.findChild<QLabel*>("refreshTaskStatus");
        QVERIFY(refresh && cancel && status);

        refresh->click();
        controller.finishMarket(false, QStringLiteral("mock provider unavailable"));

        QCOMPARE(status->text(),
                 QStringLiteral("Market refresh failed: mock provider unavailable"));
        QVERIFY(!cancel->isEnabled());
        QVERIFY(refresh->isEnabled());
        QVERIFY(window.statusBar()->currentMessage().contains(
            QStringLiteral("mock provider unavailable")));
    }

    void economicRefreshCancellationRestoresIdleUi() {
        oqt::application::EconomicDataExplorerQuery economicQuery(
            [](const QString&, const QString&, qsizetype, qint64, QString*)
                -> std::optional<oqt::application::QueryPage<oqt::data::CanonicalDataset>> {
                return std::nullopt;
            });
        FakeRefreshTaskController controller;
        MainWindow window(nullptr, &economicQuery, nullptr, nullptr, &controller);
        auto* refresh = window.findChild<QPushButton*>("refreshEconomicButton");
        auto* cancel = window.findChild<QPushButton*>("cancelRefreshButton");
        auto* status = window.findChild<QLabel*>("refreshTaskStatus");
        auto* progress = window.findChild<QProgressBar*>("refreshTaskProgress");
        QVERIFY(refresh && cancel && status && progress);

        refresh->click();
        cancel->click();
        QCOMPARE(status->text(), QStringLiteral("Cancelling economic refresh…"));
        controller.finishEconomic(false, QStringLiteral("cancelled"));

        QCOMPARE(status->text(), QStringLiteral("Economic refresh cancelled"));
        QVERIFY(!cancel->isEnabled());
        QVERIFY(refresh->isEnabled());
        QCOMPARE(progress->maximum(), 1);
        QCOMPARE(progress->value(), 0);
    }

    void marketAndEconomicRefreshControlsRemainIndependent() {
        oqt::application::MarketExplorerQuery marketQuery(
            [](const QString&, const QString&, const QString&, qsizetype, qint64, QString*)
                -> std::optional<oqt::application::QueryPage<oqt::data::CanonicalMarketSeries>> {
                return std::nullopt;
            });
        oqt::application::EconomicDataExplorerQuery economicQuery(
            [](const QString&, const QString&, qsizetype, qint64, QString*)
                -> std::optional<oqt::application::QueryPage<oqt::data::CanonicalDataset>> {
                return std::nullopt;
            });
        FakeRefreshTaskController controller;
        MainWindow window(&marketQuery, &economicQuery, nullptr, nullptr, &controller);
        auto* marketRefresh = window.findChild<QPushButton*>("refreshMarketButton");
        auto* economicRefresh = window.findChild<QPushButton*>("refreshEconomicButton");
        auto* status = window.findChild<QLabel*>("refreshTaskStatus");
        QVERIFY(marketRefresh && economicRefresh && status);

        marketRefresh->click();
        QVERIFY(!marketRefresh->isEnabled());
        QVERIFY(economicRefresh->isEnabled());

        economicRefresh->click();
        QVERIFY(!marketRefresh->isEnabled());
        QVERIFY(!economicRefresh->isEnabled());
        QCOMPARE(status->text(), QStringLiteral("Market and economic refresh running…"));

        controller.finishMarket(true);
        QVERIFY(marketRefresh->isEnabled());
        QVERIFY(!economicRefresh->isEnabled());
        controller.finishEconomic(false, QStringLiteral("mock FRED failure"));
        QVERIFY(economicRefresh->isEnabled());
        QVERIFY(marketRefresh->isEnabled());
        QVERIFY(!window.findChild<QPushButton*>("cancelRefreshButton")->isEnabled());
    }


    void refreshActionsRespectIndependentModuleAvailability() {
        oqt::application::MarketExplorerQuery marketQuery(
            [](const QString&, const QString&, const QString&, qsizetype, qint64, QString*)
                -> std::optional<oqt::application::QueryPage<oqt::data::CanonicalMarketSeries>> {
                return std::nullopt;
            });
        oqt::application::EconomicDataExplorerQuery economicQuery(
            [](const QString&, const QString&, qsizetype, qint64, QString*)
                -> std::optional<oqt::application::QueryPage<oqt::data::CanonicalDataset>> {
                return std::nullopt;
            });
        FakeRefreshTaskController controller;

        MainWindow economicOnly(nullptr, &economicQuery, nullptr, nullptr, &controller);
        QVERIFY(!economicOnly.findChild<QPushButton*>("refreshMarketButton")->isEnabled());
        QVERIFY(economicOnly.findChild<QPushButton*>("refreshEconomicButton")->isEnabled());

        MainWindow marketOnly(&marketQuery, nullptr, nullptr, nullptr, &controller);
        QVERIFY(marketOnly.findChild<QPushButton*>("refreshMarketButton")->isEnabled());
        QVERIFY(!marketOnly.findChild<QPushButton*>("refreshEconomicButton")->isEnabled());
    }

    void window_starts_in_ready_state() {
        MainWindow window;

        QVERIFY(window.statusBar() != nullptr);
        QCOMPARE(window.statusBar()->currentMessage(), QString("Ready"));
    }
};

QTEST_MAIN(MainWindowTest)

#include "tst_main_window.moc"
