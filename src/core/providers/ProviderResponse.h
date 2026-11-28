#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QByteArray>

namespace oqt::providers {

enum class ResponseStatus {
    Ok,
    Partial,
    Empty,
    Failed
};

struct ProviderResponse {
    ResponseStatus status = ResponseStatus::Failed;
    QJsonValue data = QJsonValue(QJsonValue::Undefined);
    QJsonObject metadata;
    QString errorMessage;
    bool truncated = false;
    QString nextPageCursor;

    [[nodiscard]] bool hasData() const;
    [[nodiscard]] static ProviderResponse decode(const QByteArray& json);
};

} // namespace oqt::providers
