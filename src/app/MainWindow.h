#pragma once

#include <QMainWindow>

namespace oqt::application {
class MarketExplorerQuery;
class EconomicDataExplorerQuery;
}

class QLabel;
class QLineEdit;
class QTableWidget;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    MainWindow(oqt::application::MarketExplorerQuery* marketQuery,
               oqt::application::EconomicDataExplorerQuery* economicQuery,
               QWidget* parent = nullptr);

private:
    void loadMarketSeries();
    void loadEconomicDataset();

    oqt::application::MarketExplorerQuery* m_marketQuery = nullptr;
    oqt::application::EconomicDataExplorerQuery* m_economicQuery = nullptr;

    QLineEdit* m_marketProvider = nullptr;
    QLineEdit* m_marketInstrument = nullptr;
    QLineEdit* m_marketInterval = nullptr;
    QTableWidget* m_marketTable = nullptr;
    QLabel* m_marketSummary = nullptr;

    QLineEdit* m_economicProvider = nullptr;
    QLineEdit* m_economicDataset = nullptr;
    QTableWidget* m_economicTable = nullptr;
    QLabel* m_economicSummary = nullptr;
};
