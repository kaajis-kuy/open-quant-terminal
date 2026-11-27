#include "core/providers/ProviderAdapter.h"

#include <algorithm>

namespace oqt::providers {

bool AdapterResult::isValid() const {
    const bool hasValidDataset = dataset.has_value()
        && dataset->validate().isEmpty();
    const bool hasValidMarketSeries = marketSeries.has_value()
        && marketSeries->validate().isEmpty();
    return errors.isEmpty()
        && responseStatus != ResponseStatus::Failed
        && (hasValidDataset || hasValidMarketSeries)
        && !(dataset.has_value() && marketSeries.has_value())
        && rawResult.validate().isEmpty();
}

bool ProviderAdapter::supportsOperation(const QString& operationId) const {
    const QString normalized = operationId.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }
    const QStringList operations = supportedOperations();
    return std::any_of(operations.cbegin(), operations.cend(),
        [&normalized](const QString& operation) {
            return operation.trimmed() == normalized;
        });
}

bool ProviderAdapterRegistry::registerAdapter(
    std::shared_ptr<const ProviderAdapter> adapterValue) {
    if (!adapterValue) {
        return false;
    }

    const QString id = adapterValue->providerId().trimmed();
    if (id.isEmpty() || m_adapters.contains(id)) {
        return false;
    }

    m_adapters.insert(id, std::move(adapterValue));
    return true;
}

std::shared_ptr<const ProviderAdapter> ProviderAdapterRegistry::adapter(
    const QString& providerIdValue) const {
    return m_adapters.value(providerIdValue.trimmed());
}

QStringList ProviderAdapterRegistry::providerIds() const {
    QStringList ids = m_adapters.keys();
    ids.sort(Qt::CaseSensitive);
    return ids;
}

qsizetype ProviderAdapterRegistry::size() const {
    return m_adapters.size();
}

} // namespace oqt::providers
