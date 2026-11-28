#pragma once

#include "core/providers/FredRequestBuilder.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QUrl>

namespace oqt::providers {


class FredClient final : public QObject {
    Q_OBJECT

public:
    explicit FredClient(QNetworkAccessManager* manager = nullptr,
                        QObject* parent = nullptr,
                        QUrl endpointOverride = {});

    [[nodiscard]] bool isBusy() const;

    // Fetches up to maxPages sequentially. The API key is never emitted in
    // diagnostics or signals. A partial payload includes a next-page cursor.
    [[nodiscard]] bool fetchObservations(FredObservationRequest request,
                                         QString requestId,
                                         int maxPages = 10);

    void cancel();

signals:
    void finished(const QString& requestId,
                  const QByteArray& aggregatedJson,
                  int pagesFetched,
                  const QString& errorMessage);

private:
    void requestCurrentPage();
    void handleReply(QNetworkReply* reply);
    void retryOrFinish(const QString& reason, int httpStatus = 0,
                       int retryAfterSeconds = -1);
    void finish(const QString& errorMessage = {});

    QNetworkAccessManager* m_manager = nullptr;
    QUrl m_endpointOverride;
    QPointer<QNetworkReply> m_reply;
    FredObservationRequest m_request;
    QString m_requestId;
    QJsonArray m_observations;
    qint64 m_totalCount = -1;
    int m_maxPages = 10;
    int m_pagesFetched = 0;
    int m_retryCount = 0;
    bool m_busy = false;
    quint64 m_generation = 0;
    bool m_truncated = false;
    QString m_nextPageCursor;
};

} // namespace oqt::providers
