#include "app/AppCompositionRoot.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <utility>

namespace oqt::app {
namespace {

QString normalizedPath(const QString& path) {
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return QDir::cleanPath(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
}

bool ensureParentDirectory(const QString& databasePath, QString* errorMessage) {
    const QFileInfo info(databasePath);
    if (QDir().mkpath(info.absolutePath())) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = QStringLiteral("Cannot create database directory: %1")
                            .arg(info.absolutePath());
    }
    return false;
}

} // namespace

void AppCompositionRoot::reset() {
    m_marketInstrumentCatalogQuery.reset();
    m_economicDatasetCatalogQuery.reset();
    m_marketExplorerQuery.reset();
    m_economicDataExplorerQuery.reset();
    m_fredRefresh.reset();
    m_yahooRefresh.reset();
    m_fredPipeline.reset();
    m_yahooPipeline.reset();
    m_economicIngestion.reset();
    m_marketIngestion.reset();
    m_economicStore.reset();
    m_marketStore.reset();
    m_marketDatabasePath.clear();
    m_economicDatabasePath.clear();
    m_marketStatus = {};
    m_economicStatus = {};
}

bool AppCompositionRoot::initialize(const AppCompositionConfig& config,
                                    QString* errorMessage) {
    reset();

    QString dataDirectory = config.appDataDirectory.trimmed();
    if (dataDirectory.isEmpty()) {
        dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    if (dataDirectory.isEmpty()) {
        const QString error = QStringLiteral("Application data directory is unavailable");
        m_marketStatus.errorMessage = error;
        m_economicStatus.errorMessage = error;
        if (errorMessage) *errorMessage = error;
        return false;
    }
    dataDirectory = QFileInfo(dataDirectory).absoluteFilePath();
    if (!QDir().mkpath(dataDirectory)) {
        const QString error = QStringLiteral("Cannot create application data directory");
        m_marketStatus.errorMessage = error;
        m_economicStatus.errorMessage = error;
        if (errorMessage) *errorMessage = error;
        return false;
    }

    m_marketDatabasePath = config.marketDatabasePath.trimmed().isEmpty()
        ? QDir(dataDirectory).filePath(QStringLiteral("market-data.sqlite"))
        : QFileInfo(config.marketDatabasePath).absoluteFilePath();
    m_economicDatabasePath = config.economicDatabasePath.trimmed().isEmpty()
        ? QDir(dataDirectory).filePath(QStringLiteral("economic-data.sqlite"))
        : QFileInfo(config.economicDatabasePath).absoluteFilePath();

    if (normalizedPath(m_marketDatabasePath) == normalizedPath(m_economicDatabasePath)) {
        const QString error = QStringLiteral(
            "Market and economic databases must use different files");
        m_marketStatus.errorMessage = error;
        m_economicStatus.errorMessage = error;
        if (errorMessage) *errorMessage = error;
        return false;
    }

    QString marketError;
    if (ensureParentDirectory(m_marketDatabasePath, &marketError)) {
        m_marketStore = std::make_unique<storage::SqliteMarketDataStore>(m_marketDatabasePath);
        if (m_marketStore->open(&marketError)) {
            m_marketIngestion = std::make_unique<services::MarketDataIngestionService>(*m_marketStore);
            m_yahooPipeline = std::make_unique<providers::YahooPipeline>();
            m_yahooRefresh = std::make_unique<services::YahooMarketDataRefreshService>(
                *m_yahooPipeline, *m_marketIngestion);
            connect(m_yahooRefresh.get(),
                    &services::YahooMarketDataRefreshService::finished,
                    this, &application::RefreshTaskController::marketRefreshFinished);
            m_marketExplorerQuery = std::make_unique<application::MarketExplorerQuery>(
                [store = m_marketStore.get()](const QString& providerId,
                                              const QString& instrumentId,
                                              const QString& interval,
                                              qsizetype pageSize, qint64 offset,
                                              QString* error)
                    -> std::optional<application::QueryPage<oqt::data::CanonicalMarketSeries>> {
                    auto page = store->loadSeriesPage(providerId, instrumentId, interval,
                                                      pageSize, offset, error);
                    if (!page) return std::nullopt;
                    return application::QueryPage<oqt::data::CanonicalMarketSeries>{
                        std::move(page->series), page->hasMore, page->offset, page->pageSize};
                });
            m_marketInstrumentCatalogQuery = std::make_unique<application::MarketInstrumentCatalogQuery>(
                [store = m_marketStore.get()](const QString& search, qsizetype pageSize,
                                              qint64 offset, QString* error) {
                    return store->loadInstrumentCatalog(search, pageSize, offset, error);
                });
            m_marketStatus.available = true;
        } else {
            m_marketStore.reset();
        }
    }
    m_marketStatus.errorMessage = marketError;

    QString economicError;
    if (ensureParentDirectory(m_economicDatabasePath, &economicError)) {
        m_economicStore = std::make_unique<storage::SqliteCanonicalDatasetStore>(
            m_economicDatabasePath);
        if (m_economicStore->open(&economicError)) {
            m_economicIngestion = std::make_unique<services::EconomicDataIngestionService>(
                *m_economicStore);
            m_fredPipeline = std::make_unique<providers::FredPipeline>();
            m_fredRefresh = std::make_unique<services::FredEconomicRefreshService>(
                *m_fredPipeline, *m_economicIngestion);
            connect(m_fredRefresh.get(),
                    &services::FredEconomicRefreshService::finished,
                    this, &application::RefreshTaskController::economicRefreshFinished);
            m_economicDataExplorerQuery = std::make_unique<application::EconomicDataExplorerQuery>(
                [store = m_economicStore.get()](const QString& providerId,
                                                const QString& datasetId,
                                                qsizetype pageSize, qint64 offset,
                                                QString* error)
                    -> std::optional<application::QueryPage<oqt::data::CanonicalDataset>> {
                    auto page = store->loadDatasetPage(providerId, datasetId,
                                                        pageSize, offset, error);
                    if (!page) return std::nullopt;
                    return application::QueryPage<oqt::data::CanonicalDataset>{
                        std::move(page->dataset), page->hasMore, page->offset, page->pageSize};
                });
            m_economicDatasetCatalogQuery = std::make_unique<application::EconomicDatasetCatalogQuery>(
                [store = m_economicStore.get()](const QString& search, qsizetype pageSize,
                                                qint64 offset, QString* error) {
                    return store->loadDatasetCatalog(search, pageSize, offset, error);
                });
            m_economicStatus.available = true;
        } else {
            m_economicStore.reset();
        }
    }
    m_economicStatus.errorMessage = economicError;

    if (!m_marketStatus.available && !m_economicStatus.available && errorMessage) {
        *errorMessage = QStringLiteral("No data module could be initialized (market: %1; economic: %2)")
                            .arg(m_marketStatus.errorMessage, m_economicStatus.errorMessage);
    } else if (errorMessage) {
        errorMessage->clear();
    }

    // The desktop shell remains usable even if one or both optional data modules
    // are unavailable. Their individual statuses carry the actionable errors.
    return true;
}

const ModuleStartupStatus& AppCompositionRoot::marketStatus() const {
    return m_marketStatus;
}

const ModuleStartupStatus& AppCompositionRoot::economicStatus() const {
    return m_economicStatus;
}

QString AppCompositionRoot::marketDatabasePath() const {
    return m_marketDatabasePath;
}

QString AppCompositionRoot::economicDatabasePath() const {
    return m_economicDatabasePath;
}

bool AppCompositionRoot::refreshEconomicData(
    providers::FredObservationRequest request,
    QString datasetId,
    QString requestId,
    QString* errorMessage,
    int maxPages) {
    if (!m_economicStatus.available || !m_fredRefresh) {
        if (errorMessage) *errorMessage = QStringLiteral("Economic data module is unavailable");
        return false;
    }

    const QString apiKey = qEnvironmentVariable("OQT_FRED_API_KEY").trimmed();
    if (apiKey.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("FRED API key is not configured (OQT_FRED_API_KEY)");
        }
        return false;
    }

    request.apiKey = apiKey;
    const bool started = m_fredRefresh->refresh(std::move(request),
                                                std::move(datasetId),
                                                std::move(requestId),
                                                maxPages);
    if (!started && errorMessage) {
        *errorMessage = QStringLiteral("FRED refresh could not be started; check request validity or busy state");
    } else if (errorMessage) {
        errorMessage->clear();
    }
    return started;
}

bool AppCompositionRoot::startMarketRefresh(
    const QString& providerSymbol, const QString& canonicalInstrumentId,
    const QString& interval, const QString& requestId) {
    if (!m_marketStatus.available || !m_yahooRefresh) return false;
    providers::YahooChartRequest request;
    request.symbol = providerSymbol.trimmed();
    request.interval = interval.trimmed();
    request.range = QStringLiteral("1y");
    return m_yahooRefresh->refresh(request, canonicalInstrumentId, requestId);
}

bool AppCompositionRoot::startEconomicRefresh(
    const QString& datasetId, const QString& requestId, QString* errorMessage) {
    providers::FredObservationRequest request;
    request.seriesId = datasetId.startsWith(QStringLiteral("fred."))
        ? datasetId.mid(5) : datasetId;
    return refreshEconomicData(std::move(request), datasetId, requestId, errorMessage);
}

void AppCompositionRoot::cancelMarketRefresh() {
    if (m_yahooRefresh) m_yahooRefresh->cancel();
}

void AppCompositionRoot::cancelEconomicRefresh() {
    if (m_fredRefresh) m_fredRefresh->cancel();
}

services::YahooMarketDataRefreshService* AppCompositionRoot::marketRefreshService() {
    return m_yahooRefresh.get();
}

services::FredEconomicRefreshService* AppCompositionRoot::economicRefreshService() {
    return m_fredRefresh.get();
}

application::MarketExplorerQuery* AppCompositionRoot::marketExplorerQuery() {
    return m_marketExplorerQuery.get();
}

application::EconomicDataExplorerQuery* AppCompositionRoot::economicDataExplorerQuery() {
    return m_economicDataExplorerQuery.get();
}

application::MarketInstrumentCatalogQuery* AppCompositionRoot::marketInstrumentCatalogQuery() {
    return m_marketInstrumentCatalogQuery.get();
}

application::EconomicDatasetCatalogQuery* AppCompositionRoot::economicDatasetCatalogQuery() {
    return m_economicDatasetCatalogQuery.get();
}

} // namespace oqt::app
