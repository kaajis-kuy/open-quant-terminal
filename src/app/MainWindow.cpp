#include "app/MainWindow.h"

#include "application/DataExplorerQueries.h"
#include "application/CatalogQueries.h"

#include <QHeaderView>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

namespace {

QString qualityText(oqt::data::DataStatus status) {
    switch (status) {
    case oqt::data::DataStatus::Ok: return QStringLiteral("OK");
    case oqt::data::DataStatus::Partial: return QStringLiteral("Partial");
    case oqt::data::DataStatus::Empty: return QStringLiteral("Empty");
    case oqt::data::DataStatus::Failed: return QStringLiteral("Failed");
    }
    return QStringLiteral("Unknown");
}

QTableWidget* makeTable(QWidget* parent, const QStringList& columns) {
    auto* table = new QTableWidget(parent);
    table->setColumnCount(columns.size());
    table->setHorizontalHeaderLabels(columns);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setStretchLastSection(true);
    return table;
}

void setCell(QTableWidget* table, int row, int column, const QString& value) {
    table->setItem(row, column, new QTableWidgetItem(value));
}

QString optionalNumber(const std::optional<double>& value) {
    return value ? QString::number(*value, 'g', 12) : QStringLiteral("—");
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : MainWindow(nullptr, nullptr, nullptr, nullptr, parent) {
}

MainWindow::MainWindow(oqt::application::MarketExplorerQuery* marketQuery,
                       oqt::application::EconomicDataExplorerQuery* economicQuery,
                       QWidget* parent)
    : MainWindow(marketQuery, economicQuery, nullptr, nullptr, parent) {
}

MainWindow::MainWindow(oqt::application::MarketExplorerQuery* marketQuery,
                       oqt::application::EconomicDataExplorerQuery* economicQuery,
                       oqt::application::MarketInstrumentCatalogQuery* marketCatalog,
                       oqt::application::EconomicDatasetCatalogQuery* economicCatalog,
                       QWidget* parent)
    : QMainWindow(parent),
      m_marketQuery(marketQuery),
      m_economicQuery(economicQuery),
      m_marketCatalog(marketCatalog),
      m_economicCatalog(economicCatalog) {
    setObjectName("mainWindow");
    setWindowTitle("Open Quant Terminal");
    resize(1280, 800);

    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);

    auto* title = new QLabel("Open Quant Terminal", central);
    title->setObjectName("welcomeTitle");
    title->setAlignment(Qt::AlignLeft);

    auto* subtitle = new QLabel(
        "Quantitative research terminal — market and economic data workspace",
        central
    );
    subtitle->setObjectName("welcomeSubtitle");

    auto* tabs = new QTabWidget(central);
    tabs->setObjectName("workspaceTabs");

    // Market Explorer: application query only; no provider or SQLite access in UI.
    auto* marketPage = new QWidget(tabs);
    auto* marketLayout = new QVBoxLayout(marketPage);
    m_marketCatalogSelector = new QComboBox(marketPage);
    m_marketCatalogSelector->setObjectName("marketCatalogSelector");
    m_marketCatalogSelector->addItem(QStringLiteral("Select a stored instrument…"));
    m_marketCatalogSelector->setEnabled(m_marketCatalog != nullptr);
    marketLayout->addWidget(m_marketCatalogSelector);
    QObject::connect(m_marketCatalogSelector, qOverload<int>(&QComboBox::activated),
                     this, [this](int index) { selectMarketCatalogEntry(index); });

    auto* marketInputs = new QHBoxLayout();
    m_marketProvider = new QLineEdit(QStringLiteral("yahoo_finance"), marketPage);
    m_marketProvider->setObjectName("marketProviderInput");
    m_marketProvider->setPlaceholderText("Provider ID");
    m_marketInstrument = new QLineEdit(QStringLiteral("AAPL"), marketPage);
    m_marketInstrument->setObjectName("marketInstrumentInput");
    m_marketInstrument->setPlaceholderText("Instrument ID");
    m_marketInterval = new QLineEdit(QStringLiteral("1d"), marketPage);
    m_marketInterval->setObjectName("marketIntervalInput");
    m_marketInterval->setMaximumWidth(100);
    auto* marketLoad = new QPushButton(QStringLiteral("Load series"), marketPage);
    marketLoad->setObjectName("loadMarketSeriesButton");
    marketLoad->setEnabled(m_marketQuery != nullptr);
    marketInputs->addWidget(m_marketProvider);
    marketInputs->addWidget(m_marketInstrument);
    marketInputs->addWidget(m_marketInterval);
    marketInputs->addWidget(marketLoad);
    marketLayout->addLayout(marketInputs);
    m_marketSummary = new QLabel(
        m_marketQuery ? QStringLiteral("Enter the provider, instrument ID and interval for a stored series.")
                      : QStringLiteral("Market data module is unavailable."),
        marketPage);
    m_marketSummary->setObjectName("marketSummary");
    m_marketSummary->setWordWrap(true);
    marketLayout->addWidget(m_marketSummary);
    m_marketTable = makeTable(marketPage,
        {QStringLiteral("Timestamp (UTC)"), QStringLiteral("Open"), QStringLiteral("High"),
         QStringLiteral("Low"), QStringLiteral("Close"), QStringLiteral("Adj. Close"),
         QStringLiteral("Volume")});
    m_marketTable->setObjectName("marketDataTable");
    marketLayout->addWidget(m_marketTable);
    QObject::connect(marketLoad, &QPushButton::clicked, this, [this] { loadMarketSeries(); });
    tabs->addTab(marketPage, QStringLiteral("Market Explorer"));

    // Economic Explorer: FRED datasets already persisted by the ingestion pipeline.
    auto* economicPage = new QWidget(tabs);
    auto* economicLayout = new QVBoxLayout(economicPage);
    m_economicCatalogSelector = new QComboBox(economicPage);
    m_economicCatalogSelector->setObjectName("economicCatalogSelector");
    m_economicCatalogSelector->addItem(QStringLiteral("Select a stored dataset…"));
    m_economicCatalogSelector->setEnabled(m_economicCatalog != nullptr);
    economicLayout->addWidget(m_economicCatalogSelector);
    QObject::connect(m_economicCatalogSelector, qOverload<int>(&QComboBox::activated),
                     this, [this](int index) { selectEconomicCatalogEntry(index); });

    auto* economicInputs = new QHBoxLayout();
    m_economicProvider = new QLineEdit(QStringLiteral("fred"), economicPage);
    m_economicProvider->setObjectName("economicProviderInput");
    m_economicProvider->setPlaceholderText("Provider ID");
    m_economicDataset = new QLineEdit(QStringLiteral("fred.gdp"), economicPage);
    m_economicDataset->setObjectName("economicDatasetInput");
    m_economicDataset->setPlaceholderText("Dataset ID");
    auto* economicLoad = new QPushButton(QStringLiteral("Load dataset"), economicPage);
    economicLoad->setObjectName("loadEconomicDatasetButton");
    economicLoad->setEnabled(m_economicQuery != nullptr);
    economicInputs->addWidget(m_economicProvider);
    economicInputs->addWidget(m_economicDataset);
    economicInputs->addWidget(economicLoad);
    economicLayout->addLayout(economicInputs);
    m_economicSummary = new QLabel(
        m_economicQuery ? QStringLiteral("Enter the provider and dataset ID for a stored economic dataset.")
                        : QStringLiteral("Economic data module is unavailable."),
        economicPage);
    m_economicSummary->setObjectName("economicSummary");
    m_economicSummary->setWordWrap(true);
    economicLayout->addWidget(m_economicSummary);
    m_economicTable = makeTable(economicPage,
        {QStringLiteral("Series"), QStringLiteral("Period"), QStringLiteral("Value"),
         QStringLiteral("Unit"), QStringLiteral("Frequency"), QStringLiteral("Geography")});
    m_economicTable->setObjectName("economicDataTable");
    economicLayout->addWidget(m_economicTable);
    QObject::connect(economicLoad, &QPushButton::clicked, this, [this] { loadEconomicDataset(); });
    tabs->addTab(economicPage, QStringLiteral("Economic Data Explorer"));

    rootLayout->addWidget(title);
    rootLayout->addWidget(subtitle);
    rootLayout->addWidget(tabs, 1);
    setCentralWidget(central);
    statusBar()->showMessage("Ready");
    loadCatalogSelectors();
}


void MainWindow::loadCatalogSelectors() {
    if (m_marketCatalog) {
        QString error;
        const auto page = m_marketCatalog->execute({}, 500, 0, &error);
        if (page) {
            for (const auto& entry : page->entries) {
                const QString label = QStringLiteral("%1 · %2 — %3 (%4 candles)")
                    .arg(entry.providerSymbol, entry.interval, entry.providerId)
                    .arg(entry.candleCount);
                m_marketCatalogSelector->addItem(label, QVariantList{
                    entry.providerId, entry.instrumentId, entry.interval});
            }
            if (page->hasMore) {
                m_marketCatalogSelector->setToolTip(
                    QStringLiteral("Showing the first 500 stored instruments; catalog pagination is available in the application layer"));
            }
            if (page->entries.isEmpty())
                m_marketCatalogSelector->setItemText(0, QStringLiteral("No stored instruments yet"));
            m_marketCatalogSelector->setEnabled(!page->entries.isEmpty());
        } else {
            m_marketCatalogSelector->setItemText(0, QStringLiteral("Instrument catalog unavailable"));
            m_marketCatalogSelector->setToolTip(error);
            m_marketCatalogSelector->setEnabled(false);
        }
    } else {
        m_marketCatalogSelector->setItemText(0, QStringLiteral("Instrument catalog unavailable"));
        m_marketCatalogSelector->setEnabled(false);
    }

    if (m_economicCatalog) {
        QString error;
        const auto page = m_economicCatalog->execute({}, 500, 0, &error);
        if (page) {
            for (const auto& entry : page->entries) {
                const QString label = QStringLiteral("%1 — %2 (%3 observations)")
                    .arg(entry.datasetId, entry.providerId)
                    .arg(entry.observationCount);
                m_economicCatalogSelector->addItem(label, QVariantList{
                    entry.providerId, entry.datasetId});
            }
            if (page->hasMore) {
                m_economicCatalogSelector->setToolTip(
                    QStringLiteral("Showing the first 500 stored datasets; catalog pagination is available in the application layer"));
            }
            if (page->entries.isEmpty())
                m_economicCatalogSelector->setItemText(0, QStringLiteral("No stored datasets yet"));
            m_economicCatalogSelector->setEnabled(!page->entries.isEmpty());
        } else {
            m_economicCatalogSelector->setItemText(0, QStringLiteral("Dataset catalog unavailable"));
            m_economicCatalogSelector->setToolTip(error);
            m_economicCatalogSelector->setEnabled(false);
        }
    } else {
        m_economicCatalogSelector->setItemText(0, QStringLiteral("Dataset catalog unavailable"));
        m_economicCatalogSelector->setEnabled(false);
    }
}

void MainWindow::selectMarketCatalogEntry(int index) {
    if (index <= 0 || !m_marketCatalogSelector) return;
    const QVariantList values = m_marketCatalogSelector->itemData(index).toList();
    if (values.size() != 3) return;
    m_marketProvider->setText(values.at(0).toString());
    m_marketInstrument->setText(values.at(1).toString());
    m_marketInterval->setText(values.at(2).toString());
    m_marketSummary->setText(QStringLiteral("Stored instrument selected. Load the series to inspect its data."));
}

void MainWindow::selectEconomicCatalogEntry(int index) {
    if (index <= 0 || !m_economicCatalogSelector) return;
    const QVariantList values = m_economicCatalogSelector->itemData(index).toList();
    if (values.size() != 2) return;
    m_economicProvider->setText(values.at(0).toString());
    m_economicDataset->setText(values.at(1).toString());
    m_economicSummary->setText(QStringLiteral("Stored dataset selected. Load the dataset to inspect its observations."));
}

void MainWindow::loadMarketSeries() {
    if (!m_marketQuery) return;
    QString error;
    const auto result = m_marketQuery->execute(
        m_marketProvider->text(), m_marketInstrument->text(),
        m_marketInterval->text(), &error, 1000);
    m_marketTable->setRowCount(0);
    if (!result) {
        m_marketSummary->setText(error);
        statusBar()->showMessage(QStringLiteral("Market query failed: %1").arg(error));
        return;
    }

    const auto& series = result->series;
    m_marketTable->setRowCount(static_cast<int>(series.bars.size()));
    for (qsizetype i = 0; i < series.bars.size(); ++i) {
        const auto& bar = series.bars.at(i);
        const int row = static_cast<int>(i);
        setCell(m_marketTable, row, 0, bar.timestamp.toUTC().toString(Qt::ISODateWithMs));
        setCell(m_marketTable, row, 1, QString::number(bar.open, 'g', 12));
        setCell(m_marketTable, row, 2, QString::number(bar.high, 'g', 12));
        setCell(m_marketTable, row, 3, QString::number(bar.low, 'g', 12));
        setCell(m_marketTable, row, 4, QString::number(bar.close, 'g', 12));
        setCell(m_marketTable, row, 5, optionalNumber(bar.adjustedClose));
        setCell(m_marketTable, row, 6, optionalNumber(bar.volume));
    }
    const QString retrieved = series.provenance.retrievedAt.isValid()
        ? series.provenance.retrievedAt.toUTC().toString(Qt::ISODateWithMs)
        : QStringLiteral("unknown");
    m_marketSummary->setText(
        QStringLiteral("%1 / %2 / %3 — %4 bars; quality: %5; retrieved: %6; source: %7%8")
            .arg(series.providerId, series.instrumentId, series.interval)
            .arg(series.bars.size())
            .arg(qualityText(series.quality.status), retrieved,
                 series.provenance.sourceUrl,
                 result->displayTruncated ? QStringLiteral("; showing latest 1,000 bars") : QString()));
    statusBar()->showMessage(QStringLiteral("Market series loaded"));
}

void MainWindow::loadEconomicDataset() {
    if (!m_economicQuery) return;
    QString error;
    const auto result = m_economicQuery->execute(
        m_economicProvider->text(), m_economicDataset->text(), &error, 1000);
    m_economicTable->setRowCount(0);
    if (!result) {
        m_economicSummary->setText(error);
        statusBar()->showMessage(QStringLiteral("Economic query failed: %1").arg(error));
        return;
    }

    const auto& dataset = result->dataset;
    m_economicTable->setRowCount(static_cast<int>(dataset.observations.size()));
    for (qsizetype i = 0; i < dataset.observations.size(); ++i) {
        const auto& observation = dataset.observations.at(i);
        const int row = static_cast<int>(i);
        setCell(m_economicTable, row, 0, observation.seriesId);
        setCell(m_economicTable, row, 1, observation.period.toString(Qt::ISODate));
        setCell(m_economicTable, row, 2, optionalNumber(observation.value));
        setCell(m_economicTable, row, 3, observation.unit);
        setCell(m_economicTable, row, 4, observation.frequency);
        setCell(m_economicTable, row, 5, observation.geography);
    }
    const QString retrieved = dataset.provenance.retrievedAt.isValid()
        ? dataset.provenance.retrievedAt.toUTC().toString(Qt::ISODateWithMs)
        : QStringLiteral("unknown");
    m_economicSummary->setText(
        QStringLiteral("%1 / %2 — %3 observations; quality: %4; retrieved: %5; source: %6%7")
            .arg(dataset.providerId, dataset.datasetId)
            .arg(dataset.observations.size())
            .arg(qualityText(dataset.quality.status), retrieved,
                 dataset.provenance.sourceUrl,
                 result->displayTruncated ? QStringLiteral("; showing latest 1,000 observations") : QString()));
    statusBar()->showMessage(QStringLiteral("Economic dataset loaded"));
}
