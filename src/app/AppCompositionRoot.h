#pragma once

#include "application/DataExplorerQueries.h"
#include "core/providers/FredPipeline.h"
#include "core/providers/YahooPipeline.h"
#include "core/services/EconomicDataIngestionService.h"
#include "core/services/FredEconomicRefreshService.h"
#include "core/services/MarketDataIngestionService.h"
#include "core/services/YahooMarketDataRefreshService.h"
#include "core/storage/SqliteCanonicalDatasetStore.h"
#include "core/storage/SqliteMarketDataStore.h"

#include <memory>

#include <QString>

namespace oqt::app {

struct AppCompositionConfig {
    // Empty means use QStandardPaths::AppDataLocation.
    QString appDataDirectory;
    // Empty paths resolve to market-data.sqlite and economic-data.sqlite
    // beneath appDataDirectory.
    QString marketDatabasePath;
    QString economicDatabasePath;
};

struct ModuleStartupStatus {
    bool available = false;
    QString errorMessage;
};

// Owns application services and their dependencies; UI classes remain consumers.
class AppCompositionRoot final {
public:
    AppCompositionRoot() = default;
    ~AppCompositionRoot() = default;

    AppCompositionRoot(const AppCompositionRoot&) = delete;
    AppCompositionRoot& operator=(const AppCompositionRoot&) = delete;

    // Initializes market and economic modules independently (degraded mode).
    [[nodiscard]] bool initialize(const AppCompositionConfig& config = {},
                                  QString* errorMessage = nullptr);

    [[nodiscard]] const ModuleStartupStatus& marketStatus() const;
    [[nodiscard]] const ModuleStartupStatus& economicStatus() const;
    [[nodiscard]] QString marketDatabasePath() const;
    [[nodiscard]] QString economicDatabasePath() const;

    // Reads OQT_FRED_API_KEY only when a refresh is requested. The key is not
    // retained by this object, persisted, or included in application diagnostics.
    [[nodiscard]] bool refreshEconomicData(
        oqt::providers::FredObservationRequest request,
        QString datasetId,
        QString requestId,
        QString* errorMessage = nullptr,
        int maxPages = 10);

    [[nodiscard]] services::YahooMarketDataRefreshService* marketRefreshService();
    [[nodiscard]] services::FredEconomicRefreshService* economicRefreshService();
    [[nodiscard]] application::MarketExplorerQuery* marketExplorerQuery();
    [[nodiscard]] application::EconomicDataExplorerQuery* economicDataExplorerQuery();

private:
    void reset();

    QString m_marketDatabasePath;
    QString m_economicDatabasePath;
    ModuleStartupStatus m_marketStatus;
    ModuleStartupStatus m_economicStatus;

    std::unique_ptr<application::MarketExplorerQuery> m_marketExplorerQuery;
    std::unique_ptr<application::EconomicDataExplorerQuery> m_economicDataExplorerQuery;
    std::unique_ptr<storage::SqliteMarketDataStore> m_marketStore;
    std::unique_ptr<storage::SqliteCanonicalDatasetStore> m_economicStore;
    std::unique_ptr<services::MarketDataIngestionService> m_marketIngestion;
    std::unique_ptr<services::EconomicDataIngestionService> m_economicIngestion;
    std::unique_ptr<providers::YahooPipeline> m_yahooPipeline;
    std::unique_ptr<providers::FredPipeline> m_fredPipeline;
    std::unique_ptr<services::YahooMarketDataRefreshService> m_yahooRefresh;
    std::unique_ptr<services::FredEconomicRefreshService> m_fredRefresh;
};

} // namespace oqt::app
