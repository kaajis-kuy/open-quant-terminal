#include "core/providers/FredPipeline.h"

#include <QDateTime>
#include <QJsonObject>

namespace oqt::providers {

FredPipeline::FredPipeline(FredClient* client, QObject* parent)
    : QObject(parent),
      m_client(client ? client : new FredClient(nullptr, this)) {
    qRegisterMetaType<oqt::providers::AdapterResult>();
    connect(m_client, &FredClient::finished, this,
            &FredPipeline::handleClientFinished);
}

bool FredPipeline::isBusy() const {
    return m_busy;
}

bool FredPipeline::fetchObservations(FredObservationRequest request,
                                     QString datasetId,
                                     QString requestId,
                                     int maxPages) {
    if (m_busy || datasetId.trimmed().isEmpty() || requestId.trimmed().isEmpty()) {
        return false;
    }

    ProviderRequestContext context;
    context.providerId = QStringLiteral("fred");
    context.operationId = QStringLiteral("observations");
    context.datasetId = std::move(datasetId);
    context.requestId = requestId;
    context.requestedAt = QDateTime::currentDateTimeUtc();

    // Store only non-secret request parameters for provenance/audit metadata.
    context.redactedParameters.insert(QStringLiteral("series_id"), request.seriesId);
    if (!request.units.isEmpty()) {
        context.redactedParameters.insert(QStringLiteral("units"), request.units);
    }
    if (!request.frequency.isEmpty()) {
        context.redactedParameters.insert(QStringLiteral("frequency"), request.frequency);
    }
    if (!request.aggregationMethod.isEmpty()) {
        context.redactedParameters.insert(QStringLiteral("aggregation_method"),
                                          request.aggregationMethod);
    }
    if (request.observationStart.has_value()) {
        context.redactedParameters.insert(QStringLiteral("observation_start"),
            request.observationStart->toString(Qt::ISODate));
    }
    if (request.observationEnd.has_value()) {
        context.redactedParameters.insert(QStringLiteral("observation_end"),
            request.observationEnd->toString(Qt::ISODate));
    }
    context.redactedParameters.insert(QStringLiteral("sort_order"), request.sortOrder);
    context.redactedParameters.insert(QStringLiteral("order_by"), request.orderBy);
    context.redactedParameters.insert(QStringLiteral("limit"), request.limit);
    context.redactedParameters.insert(QStringLiteral("offset"), request.offset);

    m_context = std::move(context);
    m_busy = true;
    if (!m_client->fetchObservations(std::move(request), std::move(requestId), maxPages)) {
        m_busy = false;
        m_context = {};
        return false;
    }
    return true;
}

void FredPipeline::cancel() {
    if (m_busy) {
        m_client->cancel();
    }
}

void FredPipeline::handleClientFinished(const QString& requestId,
                                        const QByteArray& aggregatedJson,
                                        int pagesFetched,
                                        const QString& errorMessage) {
    Q_UNUSED(pagesFetched);
    if (!m_busy || requestId != m_context.requestId) {
        return;
    }

    ProviderResponse response = ProviderResponse::decode(aggregatedJson);
    // Preserve transport/client errors even when a provider payload exists.
    if (!errorMessage.isEmpty() && response.errorMessage.isEmpty()) {
        response.errorMessage = errorMessage;
        response.status = response.hasData() ? ResponseStatus::Partial
                                             : ResponseStatus::Failed;
    }

    AdapterResult result = m_adapter.normalize(response, m_context);
    // fetchedAt/retrievedAt describe completed retrieval, not request start.
    const QDateTime retrievedAt = QDateTime::currentDateTimeUtc();
    result.rawResult.fetchedAt = retrievedAt;
    if (result.dataset.has_value()) {
        result.dataset->provenance.retrievedAt = retrievedAt;
    }
    m_busy = false;
    m_context = {};
    emit finished(requestId, result);
}

} // namespace oqt::providers
