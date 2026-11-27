#include "core/providers/FredClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>

#include <algorithm>

namespace oqt::providers {
namespace {

constexpr int kMaxRetries = 3;
constexpr int kTransferTimeoutMs = 15000;
constexpr int kMaxRetryDelaySeconds = 30;

int retryDelaySeconds(int retryCount) {
    return std::min(1 << std::min(retryCount, 4), kMaxRetryDelaySeconds);
}

} // namespace

FredClient::FredClient(QNetworkAccessManager* manager, QObject* parent, QUrl endpointOverride)
    : QObject(parent),
      m_manager(manager ? manager : new QNetworkAccessManager(this)),
      m_endpointOverride(std::move(endpointOverride)) {
}

bool FredClient::isBusy() const {
    return m_busy;
}

bool FredClient::fetchObservations(FredObservationRequest request,
                                   QString requestId,
                                   int maxPages) {
    if (m_busy || requestId.trimmed().isEmpty()
        || maxPages < 1 || maxPages > 100) {
        return false;
    }

    const FredBuiltRequest validation = FredRequestBuilder::observations(request);
    if (!validation.isValid()) {
        return false;
    }

    m_request = std::move(request);
    m_requestId = std::move(requestId);
    m_maxPages = maxPages;
    m_pagesFetched = 0;
    m_retryCount = 0;
    m_totalCount = -1;
    m_observations = {};
    m_truncated = false;
    m_nextPageCursor.clear();
    m_busy = true;

    requestCurrentPage();
    return true;
}

void FredClient::cancel() {
    if (!m_busy) {
        return;
    }
    const QString requestId = m_requestId;
    const int pagesFetched = m_pagesFetched;
    QPointer<QNetworkReply> reply = m_reply;
    m_busy = false;
    m_reply = nullptr;
    m_request.apiKey.clear();
    m_requestId.clear();
    if (reply) {
        reply->abort();
    }
    emit finished(requestId, {}, pagesFetched, QStringLiteral("FRED request cancelled"));
}

void FredClient::requestCurrentPage() {
    if (!m_busy) {
        return;
    }

    FredObservationRequest pageRequest = m_request;
    pageRequest.offset = m_request.offset + static_cast<int>(m_observations.size());
    const FredBuiltRequest built = FredRequestBuilder::observations(pageRequest);
    if (!built.isValid()) {
        finish(built.error);
        return;
    }

    QUrl requestUrl = built.url;
    if (m_endpointOverride.isValid() && !m_endpointOverride.isEmpty()) {
        requestUrl.setScheme(m_endpointOverride.scheme());
        requestUrl.setHost(m_endpointOverride.host());
        requestUrl.setPort(m_endpointOverride.port());
        requestUrl.setPath(m_endpointOverride.path().isEmpty()
            ? QStringLiteral("/fred/series/observations")
            : m_endpointOverride.path());
    }
    QNetworkRequest request(requestUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("OpenQuantTerminal/0.1"));
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(kTransferTimeoutMs);
    m_reply = m_manager->get(request);

    connect(m_reply, &QNetworkReply::finished, this, [this, reply = m_reply.data()] {
        if (reply) {
            handleReply(reply);
        }
    });
}

void FredClient::handleReply(QNetworkReply* reply) {
    if (!reply || !m_busy) {
        if (reply) {
            reply->deleteLater();
        }
        return;
    }

    m_reply = nullptr;
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray payload = reply->readAll();
    const auto networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    const int retryAfter = reply->rawHeader("Retry-After").toInt();
    reply->deleteLater();

    // Retry only explicitly transient HTTP statuses, or transport failures
    // where no HTTP response status was received. Qt also sets a network error
    // for HTTP 4xx responses, but those client errors must not be retried.
    if (httpStatus == 429 || httpStatus >= 500
        || (httpStatus == 0 && networkError != QNetworkReply::NoError)) {
        retryOrFinish(networkError == QNetworkReply::NoError
                          ? QStringLiteral("FRED HTTP %1").arg(httpStatus)
                          : networkErrorText,
                      httpStatus, retryAfter);
        return;
    }

    if (httpStatus < 200 || httpStatus >= 300) {
        finish(QStringLiteral("FRED HTTP %1").arg(httpStatus));
        return;
    }

    if (networkError != QNetworkReply::NoError) {
        retryOrFinish(networkErrorText, httpStatus, retryAfter);
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        finish(QStringLiteral("Invalid FRED JSON response"));
        return;
    }

    const QJsonObject root = document.object();
    if (root.contains(QStringLiteral("error_code"))
        || root.contains(QStringLiteral("error_message"))) {
        const QString message = root.value(QStringLiteral("error_message")).toString(
            QStringLiteral("FRED API returned an error"));
        finish(message);
        return;
    }

    const QJsonValue rowsValue = root.value(QStringLiteral("observations"));
    if (!rowsValue.isArray()) {
        finish(QStringLiteral("FRED response is missing observations array"));
        return;
    }

    const QJsonArray rows = rowsValue.toArray();
    if (m_totalCount < 0) {
        m_totalCount = root.value(QStringLiteral("count")).toVariant().toLongLong();
    }
    // Append each observation value; appending the array itself would nest
    // page arrays and make pagination offsets/counts incorrect.
    for (const QJsonValue& row : rows) {
        m_observations.append(row);
    }
    ++m_pagesFetched;
    m_retryCount = 0;

    const qint64 nextOffset = m_request.offset + m_observations.size();
    const bool hasMoreByCount = m_totalCount >= 0 && nextOffset < m_totalCount;

    if (!hasMoreByCount || rows.isEmpty()) {
        if (hasMoreByCount && rows.isEmpty()) {
            m_truncated = true;
            m_nextPageCursor = QStringLiteral("offset=%1").arg(nextOffset);
        }
        finish();
        return;
    }

    if (m_pagesFetched >= m_maxPages) {
        m_truncated = true;
        m_nextPageCursor = QStringLiteral("offset=%1").arg(nextOffset);
        finish();
        return;
    }

    requestCurrentPage();
}

void FredClient::retryOrFinish(const QString& reason, int httpStatus,
                               int retryAfterSeconds) {
    Q_UNUSED(httpStatus);
    if (m_retryCount >= kMaxRetries) {
        finish(reason.isEmpty() ? QStringLiteral("FRED request failed after retries")
                                : reason);
        return;
    }

    const int delay = retryAfterSeconds > 0
        ? std::min(retryAfterSeconds, kMaxRetryDelaySeconds)
        : retryDelaySeconds(m_retryCount);
    ++m_retryCount;
    QTimer::singleShot(delay * 1000, this, [this] {
        if (m_busy) {
            requestCurrentPage();
        }
    });
}

void FredClient::finish(const QString& errorMessage) {
    if (!m_busy) {
        return;
    }

    if (!errorMessage.isEmpty() && !m_observations.isEmpty()) {
        m_truncated = true;
        if (m_nextPageCursor.isEmpty()) {
            m_nextPageCursor = QStringLiteral("offset=%1")
                .arg(m_request.offset + m_observations.size());
        }
    }

    QJsonObject result;
    result.insert(QStringLiteral("count"), m_totalCount < 0
        ? static_cast<qint64>(m_observations.size()) : m_totalCount);
    result.insert(QStringLiteral("offset"), m_request.offset);
    result.insert(QStringLiteral("limit"), m_request.limit);
    result.insert(QStringLiteral("observations"), m_observations);
    result.insert(QStringLiteral("truncated"), m_truncated);
    if (!m_nextPageCursor.isEmpty()) {
        result.insert(QStringLiteral("next_cursor"), m_nextPageCursor);
    }

    QJsonObject output = result;
    if (!errorMessage.isEmpty() && !m_observations.isEmpty()) {
        output = QJsonObject{
            {QStringLiteral("data"), result},
            {QStringLiteral("error"), errorMessage}
        };
    } else if (!errorMessage.isEmpty()) {
        output.insert(QStringLiteral("error_message"), errorMessage);
    }

    const QByteArray json = QJsonDocument(output).toJson(QJsonDocument::Compact);
    const QString requestId = m_requestId;
    const int pagesFetched = m_pagesFetched;
    m_busy = false;
    m_reply = nullptr;
    m_request.apiKey.clear();
    m_requestId.clear();
    emit finished(requestId, json, pagesFetched, errorMessage);
}

} // namespace oqt::providers
