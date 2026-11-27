#include "core/data/CanonicalData.h"

#include <cmath>

namespace oqt::data {

QStringList DataQuality::validate() const {
    QStringList errors;
    if (completeness.has_value()
        && (!std::isfinite(*completeness) || *completeness < 0.0 || *completeness > 1.0)) {
        errors.append(QStringLiteral("completeness must be between 0 and 1"));
    }
    if (status == DataStatus::Partial && !isPartial) {
        errors.append(QStringLiteral("partial status requires isPartial=true"));
    }
    if (status == DataStatus::Failed && warnings.isEmpty() && nextPageCursor.isEmpty()) {
        // A failed result may have no warnings; the status itself is sufficient.
    }
    for (auto it = missingValueCounts.cbegin(); it != missingValueCounts.cend(); ++it) {
        if (it.key().trimmed().isEmpty() || it.value() < 0) {
            errors.append(QStringLiteral("missing-value counts require a non-empty key and non-negative count"));
            break;
        }
    }
    return errors;
}

QStringList Provenance::validate() const {
    QStringList errors;
    if (cacheAgeSeconds < 0) {
        errors.append(QStringLiteral("cacheAgeSeconds cannot be negative"));
    }
    if (stale && !servedFromCache) {
        errors.append(QStringLiteral("stale data must be served from cache"));
    }
    return errors;
}

QStringList CanonicalObservation::validate() const {
    QStringList errors;
    if (providerId.trimmed().isEmpty()) {
        errors.append(QStringLiteral("providerId is required"));
    }
    if (datasetId.trimmed().isEmpty()) {
        errors.append(QStringLiteral("datasetId is required"));
    }
    if (seriesId.trimmed().isEmpty()) {
        errors.append(QStringLiteral("seriesId is required"));
    }
    if (!period.isValid()) {
        errors.append(QStringLiteral("period must be a valid date"));
    }
    if (unit.trimmed().isEmpty()) {
        errors.append(QStringLiteral("unit is required"));
    }
    if (value.has_value() && !std::isfinite(*value)) {
        errors.append(QStringLiteral("value must be finite when present"));
    }
    if (!std::isfinite(scaleMultiplier) || scaleMultiplier <= 0.0) {
        errors.append(QStringLiteral("scaleMultiplier must be finite and greater than zero"));
    }
    return errors;
}

QStringList CanonicalDataset::validate() const {
    QStringList errors;
    if (providerId.trimmed().isEmpty()) {
        errors.append(QStringLiteral("providerId is required"));
    }
    if (datasetId.trimmed().isEmpty()) {
        errors.append(QStringLiteral("datasetId is required"));
    }
    if (schemaVersion.trimmed().isEmpty()) {
        errors.append(QStringLiteral("schemaVersion is required"));
    }

    for (qsizetype index = 0; index < observations.size(); ++index) {
        const QStringList observationErrors = observations.at(index).validate();
        for (const QString& error : observationErrors) {
            errors.append(QStringLiteral("observations[%1]: %2").arg(index).arg(error));
        }
        if (observations.at(index).providerId != providerId
            || observations.at(index).datasetId != datasetId) {
            errors.append(QStringLiteral("observation providerId/datasetId must match its dataset"));
        }
    }

    errors.append(provenance.validate());
    errors.append(quality.validate());
    return errors;
}

QStringList ProviderRawResult::validate() const {
    QStringList errors;
    if (providerId.trimmed().isEmpty()) {
        errors.append(QStringLiteral("providerId is required"));
    }
    if (operationId.trimmed().isEmpty()) {
        errors.append(QStringLiteral("operationId is required"));
    }
    if (responseSchemaVersion.trimmed().isEmpty()) {
        errors.append(QStringLiteral("responseSchemaVersion is required"));
    }
    if (!fetchedAt.isValid()) {
        errors.append(QStringLiteral("fetchedAt must be a valid timestamp"));
    }
    if (rawPayload.isUndefined() && rawPayloadReference.trimmed().isEmpty()) {
        errors.append(QStringLiteral("rawPayload or rawPayloadReference is required"));
    }
    return errors;
}

} // namespace oqt::data
