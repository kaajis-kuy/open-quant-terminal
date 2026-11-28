#include "core/services/FredEconomicRefreshService.h"

#include <utility>

namespace oqt::services {

FredEconomicRefreshService::FredEconomicRefreshService(
    oqt::providers::FredPipeline& pipeline,
    EconomicDataIngestionService& ingestion,
    QObject* parent)
    : QObject(parent), m_pipeline(pipeline), m_ingestion(ingestion) {
    connect(&m_pipeline, &oqt::providers::FredPipeline::finished,
            this, &FredEconomicRefreshService::handlePipelineFinished);
}

bool FredEconomicRefreshService::isBusy() const {
    return !m_activeRequestId.isEmpty();
}

void FredEconomicRefreshService::cancel() {
    if (isBusy()) {
        m_pipeline.cancel();
    }
}

bool FredEconomicRefreshService::refresh(
    oqt::providers::FredObservationRequest request,
    QString datasetId,
    QString requestId,
    int maxPages) {
    if (isBusy() || m_pipeline.isBusy()
        || datasetId.trimmed().isEmpty()
        || requestId.trimmed().isEmpty()
        || request.apiKey.trimmed().isEmpty()) {
        return false;
    }

    m_activeRequestId = requestId;
    if (!m_pipeline.fetchObservations(std::move(request),
                                      std::move(datasetId),
                                      requestId,
                                      maxPages)) {
        m_activeRequestId.clear();
        return false;
    }
    return true;
}

void FredEconomicRefreshService::handlePipelineFinished(
    const QString& requestId,
    const oqt::providers::AdapterResult& result) {
    if (!isBusy() || requestId != m_activeRequestId) {
        return;
    }

    QString errorMessage;
    const bool persisted = m_ingestion.persist(result, &errorMessage);
    m_activeRequestId.clear();
    emit finished(requestId, persisted, errorMessage, result);
}

} // namespace oqt::services
