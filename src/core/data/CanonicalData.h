#pragma once

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace oqt::data {

enum class DataStatus {
    Ok,
    Partial,
    Empty,
    Failed
};

struct DataQuality {
    DataStatus status = DataStatus::Ok;
    std::optional<double> completeness;
    bool isPartial = false;
    bool isTruncated = false;
    QHash<QString, qsizetype> missingValueCounts;
    QStringList warnings;
    QString nextPageCursor;

    [[nodiscard]] QStringList validate() const;
};

struct Provenance {
    QString sourceUrl;
    QString attributionId;
    QString providerRelease;
    QString providerVintage;
    QDateTime retrievedAt;
    qint64 cacheAgeSeconds = 0;
    bool servedFromCache = false;
    bool stale = false;

    [[nodiscard]] QStringList validate() const;
};

struct CanonicalObservation {
    QString providerId;
    QString datasetId;
    QString seriesId;
    QDate period;
    std::optional<double> value;
    QString unit;
    QString frequency;
    QString geography;
    QString seasonalAdjustment;
    QString measureType;
    QString currency;
    QString baseCurrency;
    QString quoteCurrency;
    double scaleMultiplier = 1.0;
    QString denominator;

    [[nodiscard]] QStringList validate() const;
};

struct CanonicalDataset {
    QString providerId;
    QString datasetId;
    QString schemaVersion = QStringLiteral("1.0");
    QVector<CanonicalObservation> observations;
    Provenance provenance;
    DataQuality quality;

    [[nodiscard]] QStringList validate() const;
};

struct ProviderRawResult {
    QString providerId;
    QString operationId;
    QString requestId;
    QString responseSchemaVersion;
    QDateTime fetchedAt;
    QJsonValue rawPayload;
    QString rawPayloadReference;
    QStringList redactedRequestArguments;
    QString errorMessage;

    [[nodiscard]] QStringList validate() const;
};

} // namespace oqt::data
