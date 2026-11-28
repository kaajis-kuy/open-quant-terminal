#pragma once

#include <QMainWindow>

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
               QWidget* parent = nullptr);

private:
    void loadMarketSeries();
    void loadEconomicDataset();
    void loadCatalogSelectors();
    void loadMarketCatalogPage(qint64 offset);
    void loadEconomicCatalogPage(qint64 offset);
    void selectMarketCatalogEntry(int index);
    void selectEconomicCatalogEntry(int index);

    oqt::application::MarketExplorerQuery* m_marketQuery = nullptr;
    oqt::application::EconomicDataExplorerQuery* m_economicQuery = nullptr;
    oqt::application::MarketInstrumentCatalogQuery* m_marketCatalog = nullptr;
    oqt::application::EconomicDatasetCatalogQuery* m_economicCatalog = nullptr;

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
};
