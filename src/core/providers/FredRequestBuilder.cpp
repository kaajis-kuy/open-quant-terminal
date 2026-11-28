#include "core/providers/FredRequestBuilder.h"

#include <QUrlQuery>

namespace oqt::providers {
namespace {

bool validIsoDate(const QString& value) {
    if (value.isEmpty()) {
        return true;
    }
    const QDate date = QDate::fromString(value, Qt::ISODate);
    return date.isValid() && date.toString(Qt::ISODate) == value;
}

} // namespace

FredBuiltRequest FredRequestBuilder::observations(
    const FredObservationRequest& request) {
    FredBuiltRequest result;

    if (request.apiKey.trimmed().isEmpty()) {
        result.error = QStringLiteral("FRED API key is required");
        return result;
    }
    if (request.seriesId.trimmed().isEmpty()) {
        result.error = QStringLiteral("FRED series_id is required");
        return result;
    }
    if (request.limit < 1 || request.limit > 100000) {
        result.error = QStringLiteral("FRED limit must be between 1 and 100000");
        return result;
    }
    if (request.offset < 0) {
        result.error = QStringLiteral("FRED offset cannot be negative");
        return result;
    }
    if (request.observationStart && !request.observationStart->isValid()) {
        result.error = QStringLiteral("observation_start must be a valid date");
        return result;
    }
    if (request.observationEnd && !request.observationEnd->isValid()) {
        result.error = QStringLiteral("observation_end must be a valid date");
        return result;
    }
    if (request.observationStart && request.observationEnd
        && *request.observationStart > *request.observationEnd) {
        result.error = QStringLiteral("observation_start cannot be after observation_end");
        return result;
    }
    if (!validIsoDate(request.realtimeStart) || !validIsoDate(request.realtimeEnd)) {
        result.error = QStringLiteral("realtime_start and realtime_end must use YYYY-MM-DD");
        return result;
    }
    if (!request.realtimeStart.isEmpty() && !request.realtimeEnd.isEmpty()
        && request.realtimeStart > request.realtimeEnd) {
        result.error = QStringLiteral("realtime_start cannot be after realtime_end");
        return result;
    }
    if (request.sortOrder != QStringLiteral("asc")
        && request.sortOrder != QStringLiteral("desc")) {
        result.error = QStringLiteral("sort_order must be 'asc' or 'desc'");
        return result;
    }

    result.url = QUrl(QStringLiteral("https://api.stlouisfed.org/fred/series/observations"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("api_key"), request.apiKey.trimmed());
    query.addQueryItem(QStringLiteral("file_type"), QStringLiteral("json"));
    query.addQueryItem(QStringLiteral("series_id"), request.seriesId.trimmed());

    if (request.observationStart) {
        query.addQueryItem(QStringLiteral("observation_start"),
                           request.observationStart->toString(Qt::ISODate));
    }
    if (request.observationEnd) {
        query.addQueryItem(QStringLiteral("observation_end"),
                           request.observationEnd->toString(Qt::ISODate));
    }
    if (!request.units.trimmed().isEmpty()) {
        query.addQueryItem(QStringLiteral("units"), request.units.trimmed());
    }
    if (!request.frequency.trimmed().isEmpty()) {
        query.addQueryItem(QStringLiteral("frequency"), request.frequency.trimmed());
    }
    if (!request.aggregationMethod.trimmed().isEmpty()) {
        query.addQueryItem(QStringLiteral("aggregation_method"),
                           request.aggregationMethod.trimmed());
    }
    if (request.outputType) {
        query.addQueryItem(QStringLiteral("output_type"),
                           QString::number(*request.outputType));
    }
    if (!request.realtimeStart.isEmpty()) {
        query.addQueryItem(QStringLiteral("realtime_start"), request.realtimeStart);
    }
    if (!request.realtimeEnd.isEmpty()) {
        query.addQueryItem(QStringLiteral("realtime_end"), request.realtimeEnd);
    }

    query.addQueryItem(QStringLiteral("sort_order"), request.sortOrder);
    query.addQueryItem(QStringLiteral("order_by"), request.orderBy);
    query.addQueryItem(QStringLiteral("limit"), QString::number(request.limit));
    query.addQueryItem(QStringLiteral("offset"), QString::number(request.offset));
    result.url.setQuery(query);

    QUrl redacted = result.url;
    QUrlQuery redactedQuery(redacted);
    redactedQuery.removeAllQueryItems(QStringLiteral("api_key"));
    redactedQuery.addQueryItem(QStringLiteral("api_key"), QStringLiteral("[REDACTED]"));
    redacted.setQuery(redactedQuery);
    result.redactedUrl = redacted.toString(QUrl::FullyEncoded);
    return result;
}

} // namespace oqt::providers
