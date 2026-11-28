#pragma once

#include "core/data/CanonicalData.h"

#include <QDate>
#include <QDateTime>
#include <QString>

#include <QVector>

namespace oqt::data {

struct MarketInstrumentCatalogEntry {
    QString providerId;
    QString instrumentId;
    QString providerSymbol;
    QString interval;
    QString currency;
    qint64 candleCount = 0;
    QDateTime latestCandleAt;
    QString sourceUrl;
    QDateTime retrievedAt;
    DataStatus qualityStatus = DataStatus::Empty;
    bool isPartial = false;
    bool isTruncated = false;
};

struct MarketInstrumentCatalogPage {
    QVector<MarketInstrumentCatalogEntry> entries;
    bool hasMore = false;
    qint64 offset = 0;
    qsizetype pageSize = 0;
};

struct EconomicDatasetCatalogEntry {
    QString providerId;
    QString datasetId;
    QString schemaVersion;
    qint64 observationCount = 0;
    QDate earliestPeriod;
    QDate latestPeriod;
    QString sourceUrl;
    QString attributionId;
    QDateTime retrievedAt;
    DataStatus qualityStatus = DataStatus::Empty;
    bool isPartial = false;
    bool isTruncated = false;
};

struct EconomicDatasetCatalogPage {
    QVector<EconomicDatasetCatalogEntry> entries;
    bool hasMore = false;
    qint64 offset = 0;
    qsizetype pageSize = 0;
};

} // namespace oqt::data
