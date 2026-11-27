#include "core/providers/YahooPipeline.h"

#include <QDateTime>

#include <utility>

namespace oqt::providers {

YahooPipeline::YahooPipeline(YahooClient* client, QObject* parent)
    : QObject(parent),
      m_client(client ? client : new YahooClient(nullptr, this)) {
    qRegisterMetaType<oqt::providers::AdapterResult>();
    connect(m_client, &YahooClient::finished, this,
            &YahooPipeline::handleClientFinished);
}

bool YahooPipeline::isBusy() const {
    return m_busy;
}

bool YahooPipeline::fetchChart(YahooChartRequest request,
                               QString datasetId,
                               QString requestId) {
    if (m_busy || datasetId.trimmed().isEmpty() || requestId.trimmed().isEmpty()) {
        return false;
    }

    ProviderRequestContext context;
    context.providerId = QStringLiteral("yahoo_finance");
    context.operationId = QStringLiteral("historical_ohlcv");
    context.datasetId = std::move(datasetId);
    context.requestId = requestId;
    context.requestedAt = QDateTime::currentDateTimeUtc();
    context.redactedParameters.insert(QStringLiteral("symbol"), request.symbol.trimmed());
    context.redactedParameters.insert(QStringLiteral("interval"), request.interval.trimmed());
    if (request.period1.has_value() && request.period2.has_value()) {
        context.redactedParameters.insert(QStringLiteral("period1"), *request.period1);
        context.redactedParameters.insert(QStringLiteral("period2"), *request.period2);
    } else {
        context.redactedParameters.insert(QStringLiteral("range"), request.range.trimmed());
    }
    context.redactedParameters.insert(QStringLiteral("include_adjusted_close"),
                                      request.includeAdjustedClose);
    context.redactedParameters.insert(QStringLiteral("include_events"),
                                      request.includeEvents);

    m_context = std::move(context);
    m_busy = true;
    if (!m_client->fetchChart(std::move(request), std::move(requestId))) {
        m_busy = false;
        m_context = {};
        return false;
    }
    return true;
}

void YahooPipeline::cancel() {
    if (m_busy) {
        m_client->cancel();
    }
}

void YahooPipeline::handleClientFinished(const QString& requestId,
                                         const QByteArray& payload,
                                         int httpStatus,
                                         const QString& errorMessage) {
    Q_UNUSED(httpStatus);
    if (!m_busy || requestId != m_context.requestId) {
        return;
    }

    ProviderResponse response = ProviderResponse::decode(payload);
    // Transport failures take precedence over a body that might look usable.
    if (!errorMessage.isEmpty()) {
        response.errorMessage = errorMessage;
        response.status = ResponseStatus::Failed;
    }

    AdapterResult result = m_adapter.normalize(response, m_context);
    m_busy = false;
    m_context = {};
    emit finished(requestId, result);
}

} // namespace oqt::providers
