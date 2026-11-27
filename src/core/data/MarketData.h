#pragma once

#include "core/data/CanonicalData.h"

#include <QDateTime>
#include <QString>
#include <QVector>

#include <optional>

namespace oqt::data {

// One normalized OHLCV candle. Timestamps are expected to be normalized to UTC
// by provider adapters; validation checks validity but does not rewrite them.
struct CanonicalMarketBar {
    QDateTime timestamp;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    std::optional<double> adjustedClose;
    std::optional<double> volume;

    [[nodiscard]] QStringList validate() const;
};

// A provider-neutral time series for one instrument and one bar interval.
// instrumentId is an application-owned identity; providerSymbol is the
// provider-specific symbol used to retrieve the data.
struct CanonicalMarketSeries {
    QString providerId;
    QString instrumentId;
    QString providerSymbol;
    QString interval;
    QString currency;
    QVector<CanonicalMarketBar> bars;
    Provenance provenance;
    DataQuality quality;

    [[nodiscard]] QStringList validate() const;
};

} // namespace oqt::data
