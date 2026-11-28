#include "app/MainWindow.h"
#include "application/CatalogQueries.h"
#include "application/DataExplorerQueries.h"

#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QtTest>

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

    void window_starts_in_ready_state() {
        MainWindow window;

        QVERIFY(window.statusBar() != nullptr);
        QCOMPARE(window.statusBar()->currentMessage(), QString("Ready"));
    }
};

QTEST_MAIN(MainWindowTest)

#include "tst_main_window.moc"
