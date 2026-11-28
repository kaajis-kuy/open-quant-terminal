#include "core/services/YahooMarketDataRefreshService.h"

#include <utility>

namespace oqt::services {

YahooMarketDataRefreshService::YahooMarketDataRefreshService(
    oqt::providers::YahooPipeline& pipeline,
    MarketDataIngestionService& ingestion,
    QObject* parent)
    : QObject(parent),
      m_pipeline(pipeline),
      m_ingestion(ingestion) {
    connect(&m_pipeline, &oqt::providers::YahooPipeline::finished,
            this, &YahooMarketDataRefreshService::handlePipelineFinished);
}

bool YahooMarketDataRefreshService::isBusy() const {
    return !m_activeRequestId.isEmpty();
}

void YahooMarketDataRefreshService::cancel() {
    if (isBusy()) {
        m_pipeline.cancel();
    }
}

bool YahooMarketDataRefreshService::refresh(
    oqt::providers::YahooChartRequest request,
    QString instrumentId,
    QString requestId,
    MarketDataWriteMode writeMode) {
    if (isBusy() || m_pipeline.isBusy()
        || instrumentId.trimmed().isEmpty()
        || requestId.trimmed().isEmpty()) {
        return false;
    }

    m_activeRequestId = requestId;
    m_writeMode = writeMode;
    if (!m_pipeline.fetchChart(std::move(request),
                               std::move(instrumentId),
                               requestId)) {
        m_activeRequestId.clear();
        return false;
    }
    return true;
}

void YahooMarketDataRefreshService::handlePipelineFinished(
    const QString& requestId,
    const oqt::providers::AdapterResult& result) {
    if (!isBusy() || requestId != m_activeRequestId) {
        return;
    }

    QString errorMessage;
    const bool persisted = m_ingestion.persist(result, m_writeMode, &errorMessage);
    m_activeRequestId.clear();
    emit finished(requestId, persisted, errorMessage, result);
}

} // namespace oqt::services
