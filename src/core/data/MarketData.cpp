#include "core/data/MarketData.h"

#include <cmath>

namespace oqt::data {
namespace {

bool finite(double value) {
    return std::isfinite(value);
}

} // namespace

QStringList CanonicalMarketBar::validate() const {
    QStringList errors;
    if (!timestamp.isValid()) {
        errors.append(QStringLiteral("timestamp must be valid"));
    }
    if (!finite(open) || !finite(high) || !finite(low) || !finite(close)) {
        errors.append(QStringLiteral("OHLC values must be finite"));
    } else {
        if (high < low) {
            errors.append(QStringLiteral("high cannot be lower than low"));
        }
        if (high < open || high < close) {
            errors.append(QStringLiteral("high must be greater than or equal to open and close"));
        }
        if (low > open || low > close) {
            errors.append(QStringLiteral("low must be less than or equal to open and close"));
        }
    }
    if (adjustedClose.has_value() && !finite(*adjustedClose)) {
        errors.append(QStringLiteral("adjustedClose must be finite when present"));
    }
    if (volume.has_value() && (!finite(*volume) || *volume < 0.0)) {
        errors.append(QStringLiteral("volume must be finite and non-negative when present"));
    }
    return errors;
}

QStringList CanonicalMarketSeries::validate() const {
    QStringList errors;
    if (providerId.trimmed().isEmpty()) {
        errors.append(QStringLiteral("providerId is required"));
    }
    if (instrumentId.trimmed().isEmpty()) {
        errors.append(QStringLiteral("instrumentId is required"));
    }
    if (providerSymbol.trimmed().isEmpty()) {
        errors.append(QStringLiteral("providerSymbol is required"));
    }
    if (interval.trimmed().isEmpty()) {
        errors.append(QStringLiteral("interval is required"));
    }

    QDateTime previousTimestamp;
    for (qsizetype index = 0; index < bars.size(); ++index) {
        const CanonicalMarketBar& bar = bars.at(index);
        const QStringList barErrors = bar.validate();
        for (const QString& error : barErrors) {
            errors.append(QStringLiteral("bars[%1]: %2").arg(index).arg(error));
        }
        if (bar.timestamp.isValid()) {
            if (previousTimestamp.isValid()
                && bar.timestamp.toUTC() <= previousTimestamp.toUTC()) {
                errors.append(QStringLiteral("bars must be ordered by strictly increasing timestamp"));
                break;
            }
            previousTimestamp = bar.timestamp;
        }
    }

    if (bars.isEmpty() && quality.status != DataStatus::Empty
        && quality.status != DataStatus::Partial) {
        errors.append(QStringLiteral("an empty market series must have Empty or Partial quality status"));
    }
    errors.append(provenance.validate());
    errors.append(quality.validate());
    return errors;
}

} // namespace oqt::data
