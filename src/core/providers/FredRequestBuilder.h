#pragma once

#include <QDate>
#include <QUrl>
#include <QString>

#include <optional>

namespace oqt::providers {

struct FredObservationRequest {
    QString apiKey;
    QString seriesId;
    std::optional<QDate> observationStart;
    std::optional<QDate> observationEnd;
    QString units;
    QString frequency;
    QString aggregationMethod;
    std::optional<int> outputType;
    QString realtimeStart;
    QString realtimeEnd;
    QString sortOrder = QStringLiteral("asc");
    QString orderBy = QStringLiteral("observation_date");
    int limit = 100000;
    int offset = 0;
};

struct FredBuiltRequest {
    QUrl url;
    QString redactedUrl;
    QString error;

    [[nodiscard]] bool isValid() const { return error.isEmpty() && url.isValid(); }
};

class FredRequestBuilder final {
public:
    [[nodiscard]] static FredBuiltRequest observations(
        const FredObservationRequest& request);
};

} // namespace oqt::providers
