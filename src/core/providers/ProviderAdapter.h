#pragma once

#include "core/data/CanonicalData.h"
#include "core/data/MarketData.h"
#include "core/providers/ProviderResponse.h"

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QStringList>

#include <memory>
#include <optional>

namespace oqt::providers {

struct ProviderRequestContext {
    QString providerId;
    QString operationId;
    QString datasetId;
    QString requestId;
    QString responseSchemaVersion = QStringLiteral("1.0");
    QDateTime requestedAt;
    QJsonObject redactedParameters;
};

struct AdapterResult {
    ResponseStatus responseStatus = ResponseStatus::Failed;
    std::optional<oqt::data::CanonicalDataset> dataset;
    std::optional<oqt::data::CanonicalMarketSeries> marketSeries;
    oqt::data::ProviderRawResult rawResult;
    QStringList errors;

    [[nodiscard]] bool isValid() const;
};

class ProviderAdapter {
public:
    virtual ~ProviderAdapter() = default;

    [[nodiscard]] virtual QString providerId() const = 0;
    [[nodiscard]] virtual QStringList supportedOperations() const = 0;

    [[nodiscard]] virtual AdapterResult normalize(
        const ProviderResponse& response,
        const ProviderRequestContext& context) const = 0;

    [[nodiscard]] bool supportsOperation(const QString& operationId) const;
};

class ProviderAdapterRegistry final {
public:
    // Returns false for null adapters, empty IDs, or duplicate provider IDs.
    [[nodiscard]] bool registerAdapter(std::shared_ptr<const ProviderAdapter> adapter);
    [[nodiscard]] std::shared_ptr<const ProviderAdapter> adapter(const QString& providerId) const;
    [[nodiscard]] QStringList providerIds() const;
    [[nodiscard]] qsizetype size() const;

private:
    QHash<QString, std::shared_ptr<const ProviderAdapter>> m_adapters;
};

} // namespace oqt::providers
