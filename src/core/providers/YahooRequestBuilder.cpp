#include "core/providers/YahooRequestBuilder.h"

#include <QUrlQuery>

namespace oqt::providers {
namespace {

bool supportedInterval(const QString& interval) {
    static const QStringList intervals{
        QStringLiteral("1m"), QStringLiteral("2m"), QStringLiteral("5m"),
        QStringLiteral("15m"), QStringLiteral("30m"), QStringLiteral("60m"),
        QStringLiteral("90m"), QStringLiteral("1h"), QStringLiteral("1d"),
        QStringLiteral("5d"), QStringLiteral("1wk"), QStringLiteral("1mo"),
        QStringLiteral("3mo")
    };
    return intervals.contains(interval);
}

bool supportedRange(const QString& range) {
    static const QStringList ranges{
        QStringLiteral("1d"), QStringLiteral("5d"), QStringLiteral("1mo"),
        QStringLiteral("3mo"), QStringLiteral("6mo"), QStringLiteral("1y"),
        QStringLiteral("2y"), QStringLiteral("5y"), QStringLiteral("10y"),
        QStringLiteral("ytd"), QStringLiteral("max")
    };
    return ranges.contains(range);
}

} // namespace

YahooBuiltRequest YahooRequestBuilder::chart(
    const YahooChartRequest& request, const QUrl& endpointOverride) {
    YahooBuiltRequest result;
    const QString symbol = request.symbol.trimmed();
    const QString interval = request.interval.trimmed();
    const QString range = request.range.trimmed();

    if (symbol.isEmpty()) {
        result.error = QStringLiteral("Yahoo Finance symbol is required");
        return result;
    }
    if (symbol.contains(QLatin1Char('/')) || symbol.contains(QLatin1Char('?'))
        || symbol.contains(QLatin1Char('#')) || symbol.contains(QStringLiteral(".."))) {
        result.error = QStringLiteral("Yahoo Finance symbol contains invalid path characters");
        return result;
    }
    if (!supportedInterval(interval)) {
        result.error = QStringLiteral("Unsupported Yahoo Finance chart interval");
        return result;
    }
    const bool hasPeriod1 = request.period1.has_value();
    const bool hasPeriod2 = request.period2.has_value();
    if (hasPeriod1 != hasPeriod2) {
        result.error = QStringLiteral("period1 and period2 must be supplied together");
        return result;
    }
    if (hasPeriod1) {
        if (*request.period1 < 0 || *request.period2 <= *request.period1) {
            result.error = QStringLiteral("Yahoo Finance period range is invalid");
            return result;
        }
    } else if (!supportedRange(range)) {
        result.error = QStringLiteral("Unsupported Yahoo Finance chart range");
        return result;
    }

    QUrl base = endpointOverride;
    if (!base.isValid() || base.isEmpty()) {
        base = QUrl(QStringLiteral("https://query1.finance.yahoo.com"));
    }
    if (base.scheme() != QStringLiteral("https")
        && base.scheme() != QStringLiteral("http")) {
        result.error = QStringLiteral("Yahoo Finance endpoint must use HTTP or HTTPS");
        return result;
    }
    const QString prefix = base.path().endsWith(QLatin1Char('/'))
        ? base.path().chopped(1) : base.path();
    const QString encodedSymbol = QString::fromLatin1(
        QUrl::toPercentEncoding(symbol));
    base.setPath(prefix + QStringLiteral("/v8/finance/chart/") + encodedSymbol);

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("interval"), interval);
    if (hasPeriod1) {
        query.addQueryItem(QStringLiteral("period1"), QString::number(*request.period1));
        query.addQueryItem(QStringLiteral("period2"), QString::number(*request.period2));
    } else {
        query.addQueryItem(QStringLiteral("range"), range);
    }
    query.addQueryItem(QStringLiteral("includeAdjustedClose"),
                       request.includeAdjustedClose ? QStringLiteral("true")
                                                    : QStringLiteral("false"));
    if (request.includeEvents) {
        query.addQueryItem(QStringLiteral("events"), QStringLiteral("div,splits"));
    }
    base.setQuery(query);
    result.url = base;
    result.redactedUrl = base.toString(QUrl::FullyEncoded);
    return result;
}

} // namespace oqt::providers
