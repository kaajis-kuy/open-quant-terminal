#pragma once

#include <QMainWindow>

namespace oqt::application {
class MarketExplorerQuery;
class EconomicDataExplorerQuery;
}

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

    class QLineEdit* m_marketProvider = nullptr;
    class QLineEdit* m_marketInstrument = nullptr;
    class QLineEdit* m_marketInterval = nullptr;
    class QTableWidget* m_marketTable = nullptr;
    class QLabel* m_marketSummary = nullptr;

    class QLineEdit* m_economicProvider = nullptr;
    class QLineEdit* m_economicDataset = nullptr;
    class QTableWidget* m_economicTable = nullptr;
    class QLabel* m_economicSummary = nullptr;
};
