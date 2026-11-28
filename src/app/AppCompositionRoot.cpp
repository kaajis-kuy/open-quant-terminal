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

services::YahooMarketDataRefreshService* AppCompositionRoot::marketRefreshService() {
    return m_yahooRefresh.get();
}

services::FredEconomicRefreshService* AppCompositionRoot::economicRefreshService() {
    return m_fredRefresh.get();
}

} // namespace oqt::app
