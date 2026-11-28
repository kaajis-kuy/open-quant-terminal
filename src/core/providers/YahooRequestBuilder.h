#pragma once

#include <QDateTime>
#include <QUrl>
#include <QString>

#include <optional>

namespace oqt::providers {

struct YahooChartRequest {
    QString symbol;
    QString interval = QStringLiteral("1d");
    QString range = QStringLiteral("1y");
    std::optional<qint64> period1;
    std::optional<qint64> period2;
    bool includeAdjustedClose = true;
    bool includeEvents = true;
};

struct YahooBuiltRequest {
    QUrl url;
    QString redactedUrl;
    QString error;

    [[nodiscard]] bool isValid() const { return error.isEmpty() && url.isValid(); }
};

class YahooRequestBuilder final {
public:
    [[nodiscard]] static YahooBuiltRequest chart(
        const YahooChartRequest& request,
        const QUrl& endpointOverride = {});
};

} // namespace oqt::providers
