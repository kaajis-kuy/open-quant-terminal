#include "core/providers/FredAdapter.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>

#include <cmath>

namespace oqt::providers {
namespace {

QString parameter(const QJsonObject& parameters, const QString& key,
                  const QString& fallback = {}) {
    const QJsonValue value = parameters.value(key);
    return value.isString() ? value.toString().trimmed() : fallback;
}

QStringList redactedArguments(const QJsonObject& parameters) {
    QStringList result;
    for (auto it = parameters.begin(); it != parameters.end(); ++it) {
        result.append(it.key() + QLatin1Char('=') + it.value().toVariant().toString());
    }
    result.sort(Qt::CaseSensitive);
    return result;
}

} // namespace

QString FredAdapter::providerId() const {
    return QStringLiteral("fred");
}

QStringList FredAdapter::supportedOperations() const {
    return {QStringLiteral("observations")};
}

AdapterResult FredAdapter::normalize(
    const ProviderResponse& response,
    const ProviderRequestContext& context) const {
    AdapterResult result;
    result.responseStatus = response.status;

    const QString operation = context.operationId.trimmed();
    const QString seriesId = parameter(context.redactedParameters,
                                        QStringLiteral("series_id"));
    const QString unit = parameter(context.redactedParameters,
                                   QStringLiteral("units"),
                                   QStringLiteral("unspecified"));
    const QString frequency = parameter(context.redactedParameters,
                                         QStringLiteral("frequency"),
                                         QStringLiteral("unspecified"));

    result.rawResult.providerId = providerId();
    result.rawResult.operationId = operation;
    result.rawResult.requestId = context.requestId;
    result.rawResult.responseSchemaVersion = context.responseSchemaVersion;
    result.rawResult.fetchedAt = context.requestedAt;
    result.rawResult.rawPayload = response.data;
    result.rawResult.redactedRequestArguments = redactedArguments(context.redactedParameters);
    result.rawResult.errorMessage = response.errorMessage;

    if (operation != QStringLiteral("observations")) {
        result.errors.append(QStringLiteral("Unsupported FRED operation: %1").arg(operation));
        return result;
    }
    if (context.providerId.trimmed() != providerId()) {
        result.errors.append(QStringLiteral("Request providerId must be 'fred'"));
        return result;
    }
    if (context.datasetId.trimmed().isEmpty() || seriesId.isEmpty()) {
        result.errors.append(QStringLiteral("FRED datasetId and series_id are required"));
        return result;
    }
    if (response.status == ResponseStatus::Failed) {
        result.errors.append(response.errorMessage.isEmpty()
            ? QStringLiteral("FRED response failed")
            : response.errorMessage);
        return result;
    }

    if (!response.data.isObject()) {
        result.errors.append(QStringLiteral("FRED observations response must be a JSON object"));
        return result;
    }

    const QJsonObject root = response.data.toObject();
    if (root.contains(QStringLiteral("error_message"))
        || root.contains(QStringLiteral("error_code"))) {
        const QString message = root.value(QStringLiteral("error_message")).toString(
            QStringLiteral("FRED API error (%1)").arg(
                root.value(QStringLiteral("error_code")).toVariant().toString()));
        result.rawResult.errorMessage = message;
        result.errors.append(message);
        result.responseStatus = ResponseStatus::Failed;
        return result;
    }

    const QJsonValue observationsValue = root.value(QStringLiteral("observations"));
    if (!observationsValue.isArray()) {
        result.errors.append(QStringLiteral("FRED response is missing observations array"));
        return result;
    }

    oqt::data::CanonicalDataset dataset;
    dataset.providerId = providerId();
    dataset.datasetId = context.datasetId;
    dataset.schemaVersion = QStringLiteral("1.0");
    dataset.provenance.sourceUrl = QStringLiteral("https://api.stlouisfed.org/fred/series/observations");
    dataset.provenance.attributionId = QStringLiteral("FRED");
    dataset.provenance.retrievedAt = context.requestedAt;

    const QJsonArray observations = observationsValue.toArray();
    qsizetype missingValues = 0;
    qsizetype rejectedRows = 0;

    for (qsizetype index = 0; index < observations.size(); ++index) {
        if (!observations.at(index).isObject()) {
            ++rejectedRows;
            continue;
        }

        const QJsonObject row = observations.at(index).toObject();
        const QDate period = QDate::fromString(
            row.value(QStringLiteral("date")).toString(), Qt::ISODate);
        if (!period.isValid()) {
            ++rejectedRows;
            continue;
        }

        oqt::data::CanonicalObservation observation;
        observation.providerId = providerId();
        observation.datasetId = context.datasetId;
        observation.seriesId = seriesId;
        observation.period = period;
        observation.unit = unit;
        observation.frequency = frequency;

        const QString rawValue = row.value(QStringLiteral("value")).toString().trimmed();
        if (rawValue.isEmpty() || rawValue == QStringLiteral(".")) {
            ++missingValues;
        } else {
            bool ok = false;
            const double numericValue = rawValue.toDouble(&ok);
            if (ok && std::isfinite(numericValue)) {
                observation.value = numericValue;
            } else {
                ++missingValues;
                dataset.quality.warnings.append(
                    QStringLiteral("Observation %1 has a non-numeric value").arg(index));
            }
        }

        dataset.observations.append(std::move(observation));
    }

    if (missingValues > 0) {
        dataset.quality.missingValueCounts.insert(QStringLiteral("fred_missing_or_non_numeric"), missingValues);
    }
    if (rejectedRows > 0) {
        dataset.quality.isPartial = true;
        dataset.quality.status = oqt::data::DataStatus::Partial;
        dataset.quality.warnings.append(
            QStringLiteral("%1 malformed observation row(s) were skipped").arg(rejectedRows));
    }

    const qint64 count = root.value(QStringLiteral("count")).toVariant().toLongLong();
    const qint64 offset = root.value(QStringLiteral("offset")).toVariant().toLongLong();
    if (count > 0 && offset + observations.size() < count) {
        dataset.quality.isPartial = true;
        dataset.quality.isTruncated = true;
        dataset.quality.status = oqt::data::DataStatus::Partial;
        dataset.quality.nextPageCursor = QStringLiteral("offset=%1")
            .arg(offset + observations.size());
    }

    if (response.status == ResponseStatus::Partial || response.truncated) {
        dataset.quality.isPartial = true;
        dataset.quality.isTruncated = dataset.quality.isTruncated || response.truncated;
        dataset.quality.status = ResponseStatus::Partial;
        if (!response.nextPageCursor.isEmpty()) {
            dataset.quality.nextPageCursor = response.nextPageCursor;
        }
    } else if (dataset.quality.status != oqt::data::DataStatus::Partial) {
        dataset.quality.status = dataset.observations.isEmpty()
            ? oqt::data::DataStatus::Empty
            : oqt::data::DataStatus::Ok;
    }

    if (response.status == ResponseStatus::Empty || dataset.observations.isEmpty()) {
        dataset.quality.status = oqt::data::DataStatus::Empty;
    }

    result.dataset = std::move(dataset);
    if (!result.dataset->validate().isEmpty()) {
        result.errors.append(result.dataset->validate());
    }
    return result;
}

} // namespace oqt::providers
