#include "core/providers/YahooClient.h"

#include <QNetworkRequest>

#include <utility>

namespace oqt::providers {

YahooClient::YahooClient(QNetworkAccessManager* manager, QObject* parent,
                         QUrl endpointOverride, int timeoutMs)
    : QObject(parent),
      m_manager(manager ? manager : new QNetworkAccessManager(this)),
      m_endpointOverride(std::move(endpointOverride)),
      m_timeoutMs(timeoutMs > 0 ? timeoutMs : 15000) {
}

bool YahooClient::isBusy() const {
    return m_busy;
}

bool YahooClient::fetchChart(YahooChartRequest request, QString requestId) {
    if (m_busy || requestId.trimmed().isEmpty()) {
        return false;
    }

    const YahooBuiltRequest built = YahooRequestBuilder::chart(
        request, m_endpointOverride);
    if (!built.isValid()) {
        return false;
    }

    QNetworkRequest networkRequest(built.url);
    networkRequest.setHeader(QNetworkRequest::UserAgentHeader,
                             QStringLiteral("OpenQuantTerminal/0.1"));
    networkRequest.setRawHeader("Accept", "application/json");
    networkRequest.setTransferTimeout(m_timeoutMs);

    m_requestId = std::move(requestId);
    m_busy = true;
    QPointer<QNetworkReply> reply = m_manager->get(networkRequest);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (reply) {
            handleReply(reply.data());
        }
    });
    return true;
}

void YahooClient::cancel() {
    if (!m_busy) {
        return;
    }

    const QString requestId = m_requestId;
    QPointer<QNetworkReply> reply = m_reply;
    m_reply = nullptr;
    m_requestId.clear();
    m_busy = false;
    if (reply) {
        reply->abort();
    }
    emit finished(requestId, {}, 0, QStringLiteral("Yahoo Finance request cancelled"));
}

void YahooClient::handleReply(QNetworkReply* reply) {
    // Only the currently-owned reply may complete the active request.
    if (!reply || !m_busy || reply != m_reply.data()) {
        if (reply) {
            reply->deleteLater();
        }
        return;
    }

    m_reply = nullptr;
    const QString requestId = m_requestId;
    const int status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray payload = reply->readAll();
    const auto networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    reply->deleteLater();

    m_busy = false;
    m_requestId.clear();

    QString error;
    if (status < 200 || status >= 300) {
        error = status == 0
            ? (networkError == QNetworkReply::NoError
                ? QStringLiteral("Yahoo Finance returned no HTTP status")
                : networkErrorText)
            : QStringLiteral("Yahoo Finance HTTP %1").arg(status);
    } else if (networkError != QNetworkReply::NoError) {
        error = networkErrorText;
    } else if (payload.trimmed().isEmpty()) {
        error = QStringLiteral("Yahoo Finance returned an empty response");
    }

    emit finished(requestId, payload, status, error);
}

} // namespace oqt::providers
