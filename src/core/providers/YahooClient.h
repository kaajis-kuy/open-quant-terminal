#pragma once

#include "core/providers/YahooRequestBuilder.h"

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QUrl>

namespace oqt::providers {

class YahooClient final : public QObject {
    Q_OBJECT

public:
    explicit YahooClient(QNetworkAccessManager* manager = nullptr,
                         QObject* parent = nullptr,
                         QUrl endpointOverride = {},
                         int timeoutMs = 15000);

    [[nodiscard]] bool isBusy() const;
    [[nodiscard]] bool fetchChart(YahooChartRequest request, QString requestId);
    void cancel();

signals:
    void finished(const QString& requestId, const QByteArray& payload,
                  int httpStatus, const QString& errorMessage);

private:
    void handleReply(QNetworkReply* reply);

    QNetworkAccessManager* m_manager = nullptr;
    QUrl m_endpointOverride;
    QPointer<QNetworkReply> m_reply;
    QString m_requestId;
    int m_timeoutMs = 15000;
    bool m_busy = false;
};

} // namespace oqt::providers
