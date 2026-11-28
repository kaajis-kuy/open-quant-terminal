#pragma once

#include "core/data/CatalogData.h"

#include <functional>
#include <limits>
#include <optional>
#include <utility>

namespace oqt::application {

class MarketInstrumentCatalogQuery final {
public:
    using Loader = std::function<std::optional<oqt::data::MarketInstrumentCatalogPage>(
        const QString&, qsizetype, qint64, QString*)>;

    explicit MarketInstrumentCatalogQuery(Loader loader) : m_loader(std::move(loader)) {}

    [[nodiscard]] std::optional<oqt::data::MarketInstrumentCatalogPage> execute(
        const QString& searchText = {}, qsizetype pageSize = 100,
        qint64 offset = 0, QString* errorMessage = nullptr) const {
        if (!m_loader) {
            if (errorMessage) *errorMessage = QStringLiteral("Market catalog is unavailable");
            return std::nullopt;
        }
        if (pageSize <= 0 || pageSize > 500 || offset < 0
            || offset > std::numeric_limits<qint64>::max() - pageSize - 1) {
            if (errorMessage) *errorMessage = QStringLiteral("Catalog page size must be 1..500 and offset must be non-negative");
            return std::nullopt;
        }
        auto result = m_loader(searchText.trimmed(), pageSize, offset, errorMessage);
        if (!result && errorMessage && errorMessage->isEmpty())
            *errorMessage = QStringLiteral("Market catalog query failed");
        if (result && errorMessage) errorMessage->clear();
        return result;
    }

private:
    Loader m_loader;
};

class EconomicDatasetCatalogQuery final {
public:
    using Loader = std::function<std::optional<oqt::data::EconomicDatasetCatalogPage>(
        const QString&, qsizetype, qint64, QString*)>;

    explicit EconomicDatasetCatalogQuery(Loader loader) : m_loader(std::move(loader)) {}

    [[nodiscard]] std::optional<oqt::data::EconomicDatasetCatalogPage> execute(
        const QString& searchText = {}, qsizetype pageSize = 100,
        qint64 offset = 0, QString* errorMessage = nullptr) const {
        if (!m_loader) {
            if (errorMessage) *errorMessage = QStringLiteral("Economic catalog is unavailable");
            return std::nullopt;
        }
        if (pageSize <= 0 || pageSize > 500 || offset < 0
            || offset > std::numeric_limits<qint64>::max() - pageSize - 1) {
            if (errorMessage) *errorMessage = QStringLiteral("Catalog page size must be 1..500 and offset must be non-negative");
            return std::nullopt;
        }
        auto result = m_loader(searchText.trimmed(), pageSize, offset, errorMessage);
        if (!result && errorMessage && errorMessage->isEmpty())
            *errorMessage = QStringLiteral("Economic catalog query failed");
        if (result && errorMessage) errorMessage->clear();
        return result;
    }

private:
    Loader m_loader;
};

} // namespace oqt::application
