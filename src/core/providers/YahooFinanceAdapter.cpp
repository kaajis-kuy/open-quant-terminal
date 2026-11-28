#include "core/providers/YahooFinanceAdapter.h"

#include "core/data/MarketData.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <limits>

namespace oqt::providers {
namespace {

QString parameter(const QJsonObject& parameters, const QString& key) {
    const QJsonValue value = parameters.value(key);
    return value.isString() ? value.toString().trimmed()
                            : value.toVariant().toString().trimmed();
}

QStringList redactedArguments(const QJsonObject& parameters) {
    QStringList result;
    for (auto it = parameters.begin(); it != parameters.end(); ++it) {
        const QString key = it.key();
        const QString normalizedKey = key.toLower();
        const bool sensitive = normalizedKey.contains(QStringLiteral("api_key"))
            || normalizedKey == QStringLiteral("apikey")
            || normalizedKey.contains(QStringLiteral("token"))
            || normalizedKey.contains(QStringLiteral("secret"))
            || normalizedKey.contains(QStringLiteral("authorization"))
            || normalizedKey.contains(QStringLiteral("cookie"));
        result.append(key + QLatin1Char('=')
            + (sensitive ? QStringLiteral("[REDACTED]")
                         : it.value().toVariant().toString()));
    }
    result.sort(Qt::CaseSensitive);
    return result;
}

std::optional<double> optionalNumber(const QJsonValue& value) {
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double number = value.toDouble();
    return std::isfinite(number) ? std::optional<double>(number) : std::nullopt;
}

} // namespace

QString YahooFinanceAdapter::providerId() const {
    return QStringLiteral("yahoo_finance");
}

QStringList YahooFinanceAdapter::supportedOperations() const {
    return {QStringLiteral("historical_ohlcv")};
}

AdapterResult YahooFinanceAdapter::normalize(
    const ProviderResponse& response,
    const ProviderRequestContext& context) const {
    AdapterResult result;
    result.responseStatus = response.status;

    const QString operation = context.operationId.trimmed();
    const QString symbol = parameter(context.redactedParameters, QStringLiteral("symbol"));
    const QString interval = parameter(context.redactedParameters, QStringLiteral("interval"));

    result.rawResult.providerId = providerId();
    result.rawResult.operationId = operation;
    result.rawResult.requestId = context.requestId;
    result.rawResult.responseSchemaVersion = context.responseSchemaVersion;
    result.rawResult.fetchedAt = context.requestedAt;
    result.rawResult.rawPayload = response.data;
    result.rawResult.redactedRequestArguments = redactedArguments(context.redactedParameters);
    result.rawResult.errorMessage = response.errorMessage;

    if (operation != QStringLiteral("historical_ohlcv")) {
        result.errors.append(QStringLiteral("Unsupported Yahoo Finance operation: %1").arg(operation));
        return result;
    }
    if (context.providerId.trimmed() != providerId()) {
        result.errors.append(QStringLiteral("Request providerId must be 'yahoo_finance'"));
        return result;
    }
    if (context.datasetId.trimmed().isEmpty() || symbol.isEmpty() || interval.isEmpty()) {
        result.errors.append(QStringLiteral("Yahoo Finance datasetId, symbol and interval are required"));
        return result;
    }
    if (response.status == ResponseStatus::Failed) {
        result.errors.append(response.errorMessage.isEmpty()
            ? QStringLiteral("Yahoo Finance response failed")
            : response.errorMessage);
        return result;
    }
    if (!response.data.isObject()) {
        result.errors.append(QStringLiteral("Yahoo Finance chart response must be a JSON object"));
        return result;
    }

    const QJsonObject root = response.data.toObject();
    const QJsonObject chart = root.value(QStringLiteral("chart")).toObject();
    if (chart.isEmpty()) {
        result.errors.append(QStringLiteral("Yahoo Finance response is missing chart object"));
        return result;
    }

    const QJsonValue chartError = chart.value(QStringLiteral("error"));
    if (!chartError.isNull() && !chartError.isUndefined()) {
        const QJsonObject error = chartError.toObject();
        const QString message = error.value(QStringLiteral("description")).toString(
            error.value(QStringLiteral("code")).toString(
                QStringLiteral("Yahoo Finance chart API error")));
        result.rawResult.errorMessage = message;
        result.errors.append(message);
        result.responseStatus = ResponseStatus::Failed;
        return result;
    }

    const QJsonArray results = chart.value(QStringLiteral("result")).toArray();
    if (results.size() > 1) {
        result.errors.append(QStringLiteral("Yahoo Finance returned multiple chart results; one symbol was expected"));
        return result;
    }

    oqt::data::CanonicalMarketSeries series;
    series.providerId = providerId();
    series.instrumentId = context.datasetId;
    series.providerSymbol = symbol;
    series.interval = interval;
    series.provenance.sourceUrl = QStringLiteral("https://query1.finance.yahoo.com/v8/finance/chart/");
    series.provenance.attributionId = QStringLiteral("Yahoo Finance");
    series.provenance.retrievedAt = context.requestedAt;

    if (results.isEmpty() || !results.first().isObject()) {
        series.quality.status = oqt::data::DataStatus::Empty;
        series.quality.warnings.append(QStringLiteral("Yahoo Finance returned no chart result"));
        result.responseStatus = ResponseStatus::Empty;
        result.marketSeries = std::move(series);
        if (!result.marketSeries->validate().isEmpty()) {
            result.errors.append(result.marketSeries->validate());
        }
        return result;
    }

    const QJsonObject chartResult = results.first().toObject();
    const QJsonObject meta = chartResult.value(QStringLiteral("meta")).toObject();
    const QString returnedSymbol = meta.value(QStringLiteral("symbol")).toString();
    if (!returnedSymbol.isEmpty() && returnedSymbol.compare(symbol, Qt::CaseInsensitive) != 0) {
        result.errors.append(QStringLiteral("Yahoo Finance response symbol does not match requested symbol"));
        return result;
    }
    series.currency = meta.value(QStringLiteral("currency")).toString();

    const QJsonArray timestamps = chartResult.value(QStringLiteral("timestamp")).toArray();
    const QJsonObject indicators = chartResult.value(QStringLiteral("indicators")).toObject();
    const QJsonArray quotes = indicators.value(QStringLiteral("quote")).toArray();
    if (quotes.isEmpty() || !quotes.first().isObject()) {
        if (timestamps.isEmpty()) {
            series.quality.status = oqt::data::DataStatus::Empty;
            result.responseStatus = ResponseStatus::Empty;
        } else {
            result.errors.append(QStringLiteral("Yahoo Finance response is missing quote arrays"));
            return result;
        }
    }

    const QJsonObject quote = quotes.isEmpty() ? QJsonObject() : quotes.first().toObject();
    const QJsonArray opens = quote.value(QStringLiteral("open")).toArray();
    const QJsonArray highs = quote.value(QStringLiteral("high")).toArray();
    const QJsonArray lows = quote.value(QStringLiteral("low")).toArray();
    const QJsonArray closes = quote.value(QStringLiteral("close")).toArray();
    const QJsonArray volumes = quote.value(QStringLiteral("volume")).toArray();

    QJsonArray adjustedCloses;
    const QJsonArray adjusted = indicators.value(QStringLiteral("adjclose")).toArray();
    if (!adjusted.isEmpty() && adjusted.first().isObject()) {
        adjustedCloses = adjusted.first().toObject()
            .value(QStringLiteral("adjclose")).toArray();
    }

    qsizetype rejectedRows = 0;
    qsizetype missingVolume = 0;
    qsizetype missingAdjustedClose = 0;
    const qsizetype count = timestamps.size();
    const bool arraysMatch = opens.size() == count && highs.size() == count
        && lows.size() == count && closes.size() == count;
    const bool optionalArraysMatch = (volumes.isEmpty() || volumes.size() == count)
        && (adjustedCloses.isEmpty() || adjustedCloses.size() == count);
    if (!arraysMatch) {
        series.quality.isPartial = true;
        series.quality.warnings.append(QStringLiteral("Yahoo Finance OHLC arrays have inconsistent lengths"));
    }
    if (!optionalArraysMatch) {
        series.quality.isPartial = true;
        series.quality.warnings.append(QStringLiteral("Yahoo Finance optional indicator arrays have inconsistent lengths"));
    }

    for (qsizetype index = 0; index < count; ++index) {
        const QJsonValue timestampValue = timestamps.at(index);
        if (!timestampValue.isDouble()
            || !std::isfinite(timestampValue.toDouble())
            || std::floor(timestampValue.toDouble()) != timestampValue.toDouble()
            || timestampValue.toDouble() < static_cast<double>(std::numeric_limits<qint64>::min())
            || timestampValue.toDouble() > static_cast<double>(std::numeric_limits<qint64>::max())
            || index >= opens.size() || index >= highs.size()
            || index >= lows.size() || index >= closes.size()) {
            ++rejectedRows;
            continue;
        }

        const auto open = optionalNumber(opens.at(index));
        const auto high = optionalNumber(highs.at(index));
        const auto low = optionalNumber(lows.at(index));
        const auto close = optionalNumber(closes.at(index));
        if (!open || !high || !low || !close) {
            ++rejectedRows;
            continue;
        }

        oqt::data::CanonicalMarketBar bar;
        bar.timestamp = QDateTime::fromSecsSinceEpoch(
            static_cast<qint64>(timestampValue.toDouble()), Qt::UTC);
        if (!bar.timestamp.isValid()) {
            ++rejectedRows;
            continue;
        }
        bar.open = *open;
        bar.high = *high;
        bar.low = *low;
        bar.close = *close;

        if (index < adjustedCloses.size()) {
            bar.adjustedClose = optionalNumber(adjustedCloses.at(index));
        }
        if (index < volumes.size()) {
            bar.volume = optionalNumber(volumes.at(index));
        }
        if (!bar.volume) {
            ++missingVolume;
        }
        if (index < adjustedCloses.size() && !bar.adjustedClose) {
            ++missingAdjustedClose;
        }

        const QStringList barErrors = bar.validate();
        if (!barErrors.isEmpty()) {
            ++rejectedRows;
            continue;
        }
        series.bars.append(std::move(bar));
    }

    if (rejectedRows > 0 || !arraysMatch || !optionalArraysMatch) {
        series.quality.isPartial = true;
        series.quality.status = oqt::data::DataStatus::Partial;
        series.quality.warnings.append(
            QStringLiteral("%1 malformed or incomplete market bar(s) were skipped").arg(rejectedRows));
    }
    if (missingVolume > 0) {
        series.quality.missingValueCounts.insert(QStringLiteral("volume"), missingVolume);
    }
    if (missingAdjustedClose > 0) {
        series.quality.missingValueCounts.insert(QStringLiteral("adjusted_close"), missingAdjustedClose);
    }
    if (response.status == ResponseStatus::Partial || response.truncated) {
        series.quality.isPartial = true;
        series.quality.isTruncated = response.truncated;
        series.quality.status = oqt::data::DataStatus::Partial;
        if (!response.errorMessage.isEmpty()) {
            series.quality.warnings.append(response.errorMessage);
        }
        series.quality.nextPageCursor = response.nextPageCursor;
    } else if (series.quality.status != oqt::data::DataStatus::Partial) {
        series.quality.status = series.bars.isEmpty()
            ? oqt::data::DataStatus::Empty
            : oqt::data::DataStatus::Ok;
    }

    result.rawResult.rawPayload = response.data;
    // Market series is deliberately separate from the scalar CanonicalDataset.
    // It is returned through the marketSeries member of AdapterResult.
    result.marketSeries = std::move(series);
    if (!result.marketSeries->validate().isEmpty()) {
        result.errors.append(result.marketSeries->validate());
    }
    return result;
}

} // namespace oqt::providers
