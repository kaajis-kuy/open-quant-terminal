#include "core/services/ProviderNormalizationService.h"

namespace oqt::services {

ProviderNormalizationService::ProviderNormalizationService(
    const oqt::providers::ProviderAdapterRegistry& registry)
    : m_registry(registry) {
}

oqt::providers::AdapterResult ProviderNormalizationService::normalize(
    const oqt::providers::ProviderResponse& response,
    const oqt::providers::ProviderRequestContext& context) const {
    using namespace oqt::providers;

    const auto failure = [&context](const QString& message) {
        AdapterResult result;
        result.responseStatus = ResponseStatus::Failed;
        result.rawResult.providerId = context.providerId.trimmed();
        result.rawResult.operationId = context.operationId.trimmed();
        result.rawResult.requestId = context.requestId;
        result.rawResult.responseSchemaVersion = context.responseSchemaVersion;
        result.rawResult.fetchedAt = context.requestedAt;
        result.rawResult.errorMessage = message;
        result.errors.append(message);
        return result;
    };

    const QString providerId = context.providerId.trimmed();
    const QString operationId = context.operationId.trimmed();
    if (providerId.isEmpty() || operationId.isEmpty()) {
        return failure(QStringLiteral("Provider ID and operation ID are required"));
    }

    const auto adapter = m_registry.adapter(providerId);
    if (!adapter) {
        return failure(QStringLiteral("No adapter registered for provider: %1").arg(providerId));
    }
    if (!adapter->supportsOperation(operationId)) {
        return failure(QStringLiteral("Provider '%1' does not support operation '%2'")
                           .arg(providerId, operationId));
    }

    AdapterResult result = adapter->normalize(response, context);
    if (result.rawResult.providerId != adapter->providerId()
        || result.rawResult.operationId != operationId
        || result.rawResult.requestId != context.requestId) {
        const QString message = QStringLiteral(
            "Provider adapter returned inconsistent request identity metadata");
        result.responseStatus = ResponseStatus::Failed;
        result.errors.append(message);
        result.rawResult.errorMessage = message;
    }
    return result;
}

} // namespace oqt::services
