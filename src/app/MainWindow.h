#pragma once

#include <QMainWindow>

#include <functional>

namespace oqt::services {
class YahooMarketDataRefreshService;
class FredEconomicRefreshService;
}

namespace oqt::application {
class MarketExplorerQuery;
class EconomicDataExplorerQuery;
class MarketInstrumentCatalogQuery;
class EconomicDatasetCatalogQuery;
}

class QLabel;
class QLineEdit;
class QComboBox;
class QTableWidget;
class QPushButton;
class QProgressBar;
class OqtChartWidget;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    MainWindow(oqt::application::MarketExplorerQuery* marketQuery,
               oqt::application::EconomicDataExplorerQuery* economicQuery,
               QWidget* parent = nullptr);
    MainWindow(oqt::application::MarketExplorerQuery* marketQuery,
               oqt::application::EconomicDataExplorerQuery* economicQuery,
               oqt::application::MarketInstrumentCatalogQuery* marketCatalog,
               oqt::application::EconomicDatasetCatalogQuery* economicCatalog,
               oqt::services::YahooMarketDataRefreshService* marketRefresh,
               oqt::services::FredEconomicRefreshService* economicRefresh,
               std::function<bool(const QString&, const QString&, QString*)> economicRefreshStarter,
               QWidget* parent = nullptr);

private:
    void loadMarketSeries();
    void loadEconomicDataset();
    void loadCatalogSelectors();
    void loadMarketCatalogPage(qint64 offset);
    void loadEconomicCatalogPage(qint64 offset);
    void selectMarketCatalogEntry(int index);
    void selectEconomicCatalogEntry(int index);
    void updateRefreshTaskUi();
    void handleMarketRefreshFinished(const QString& requestId, bool persisted,
                                     const QString& errorMessage,
                                     const oqt::providers::AdapterResult& result);
    void handleEconomicRefreshFinished(const QString& requestId, bool persisted,
                                       const QString& errorMessage,
                                       const oqt::providers::AdapterResult& result);

    oqt::application::MarketExplorerQuery* m_marketQuery = nullptr;
    oqt::application::EconomicDataExplorerQuery* m_economicQuery = nullptr;
    oqt::application::MarketInstrumentCatalogQuery* m_marketCatalog = nullptr;
    oqt::application::EconomicDatasetCatalogQuery* m_economicCatalog = nullptr;
    oqt::services::YahooMarketDataRefreshService* m_marketRefresh = nullptr;
    oqt::services::FredEconomicRefreshService* m_economicRefresh = nullptr;
    std::function<bool(const QString&, const QString&, QString*)> m_economicRefreshStarter;
    QString m_marketRequestId;
    QString m_economicRequestId;
    bool m_marketCancelRequested = false;
    bool m_economicCancelRequested = false;
    QPushButton* m_marketRefreshButton = nullptr;
    QPushButton* m_economicRefreshButton = nullptr;
    QPushButton* m_cancelRefreshButton = nullptr;
    QProgressBar* m_refreshProgress = nullptr;
    QLabel* m_refreshTaskLabel = nullptr;

    QComboBox* m_marketCatalogSelector = nullptr;
    QPushButton* m_marketCatalogPrevious = nullptr;
    QPushButton* m_marketCatalogNext = nullptr;
    QLabel* m_marketCatalogPageLabel = nullptr;
    qint64 m_marketCatalogOffset = 0;
    bool m_marketCatalogHasMore = false;
    QLineEdit* m_marketProvider = nullptr;
    QLineEdit* m_marketInstrument = nullptr;
    QLineEdit* m_marketInterval = nullptr;
    QTableWidget* m_marketTable = nullptr;
    QLabel* m_marketSummary = nullptr;
    OqtChartWidget* m_marketChart = nullptr;

    QComboBox* m_economicCatalogSelector = nullptr;
    QPushButton* m_economicCatalogPrevious = nullptr;
    QPushButton* m_economicCatalogNext = nullptr;
    QLabel* m_economicCatalogPageLabel = nullptr;
    qint64 m_economicCatalogOffset = 0;
    bool m_economicCatalogHasMore = false;
    QLineEdit* m_economicProvider = nullptr;
    QLineEdit* m_economicDataset = nullptr;
    QTableWidget* m_economicTable = nullptr;
    QLabel* m_economicSummary = nullptr;
    OqtChartWidget* m_economicChart = nullptr;
};
