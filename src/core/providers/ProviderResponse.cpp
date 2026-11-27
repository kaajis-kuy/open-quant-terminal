#include "core/providers/ProviderResponse.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

namespace oqt::providers {
namespace {

bool isEmptyPayload(const QJsonValue& value) {
    if (value.isUndefined() || value.isNull()) {
        return true;
    }
    if (value.isArray()) {
        return value.toArray().isEmpty();
    }
    if (value.isObject()) {
        return value.toObject().isEmpty();
    }
    return false;
}

QString readError(const QJsonObject& object) {
    const QJsonValue error = object.value(QStringLiteral("error"));
    if (error.isString()) {
        return error.toString();
    }
    if (object.value(QStringLiteral("ok")).isBool()
        && !object.value(QStringLiteral("ok")).toBool()) {
        return QStringLiteral("Provider reported ok=false");
    }
    return {};
}

} // namespace

bool ProviderResponse::hasData() const {
    return !isEmptyPayload(data);
}

ProviderResponse ProviderResponse::decode(const QByteArray& json) {
    ProviderResponse response;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);

    if (parseError.error != QJsonParseError::NoError || document.isNull()) {
        response.errorMessage = parseError.errorString();
        response.status = ResponseStatus::Failed;
        return response;
    }

    if (document.isArray()) {
        response.data = document.array();
    } else if (document.isObject()) {
        const QJsonObject root = document.object();
        response.metadata = root.value(QStringLiteral("meta")).toObject();
        if (response.metadata.isEmpty()) {
            response.metadata = root.value(QStringLiteral("metadata")).toObject();
        }

        if (root.contains(QStringLiteral("data"))) {
            response.data = root.value(QStringLiteral("data"));
        } else {
            // A provider may return a domain object directly rather than an envelope.
            response.data = root;
        }

        response.errorMessage = readError(root);
        response.truncated = root.value(QStringLiteral("truncated")).toBool()
            || response.metadata.value(QStringLiteral("truncated")).toBool();
        response.nextPageCursor = root.value(QStringLiteral("next_cursor")).toString();
        if (response.nextPageCursor.isEmpty()) {
            response.nextPageCursor = response.metadata.value(QStringLiteral("next_cursor")).toString();
        }
    } else {
        response.errorMessage = QStringLiteral("Provider response must be a JSON object or array");
        response.status = ResponseStatus::Failed;
        return response;
    }

    const bool hasPayload = response.hasData();
    if (!response.errorMessage.isEmpty()) {
        response.status = hasPayload ? ResponseStatus::Partial : ResponseStatus::Failed;
    } else if (response.truncated) {
        response.status = ResponseStatus::Partial;
    } else if (!hasPayload) {
        response.status = ResponseStatus::Empty;
    } else {
        response.status = ResponseStatus::Ok;
    }

    return response;
}

} // namespace oqt::providers
