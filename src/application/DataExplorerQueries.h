#pragma once

#include "core/data/CanonicalData.h"
#include "core/data/MarketData.h"

#include <functional>
#include <optional>
#include <utility>

namespace oqt::application {

struct MarketSeriesReadModel {
    oqt::data::CanonicalMarketSeries series;
    bool displayTruncated = false;
};

struct EconomicDatasetReadModel {
    oqt::data::CanonicalDataset dataset;
    bool displayTruncated = false;
};

// Application-facing read boundary. Infrastructure is supplied as a callback
// by the composition root; feature UI never receives a concrete SQLite store.
class MarketExplorerQuery final {
public:
    using Loader = std::function<std::optional<oqt::data::CanonicalMarketSeries>(
        const QString&, const QString&, const QString&, QString*)>;

    explicit MarketExplorerQuery(Loader loader) : m_loader(std::move(loader)) {}

    [[nodiscard]] std::optional<MarketSeriesReadModel> execute(
        const QString& providerId, const QString& instrumentId,
        const QString& interval, QString* errorMessage = nullptr,
        qsizetype displayLimit = 1000) const {
        const QString provider = providerId.trimmed();
        const QString instrument = instrumentId.trimmed();
        const QString barInterval = interval.trimmed();
        if (provider.isEmpty() || instrument.isEmpty() || barInterval.isEmpty()) {
            if (errorMessage) *errorMessage = QStringLiteral("Provider, instrument and interval are required");
            return std::nullopt;
        }
        if (!m_loader) {
            if (errorMessage) *errorMessage = QStringLiteral("Market data query is unavailable");
            return std::nullopt;
        }
        if (displayLimit <= 0) {
            if (errorMessage) *errorMessage = QStringLiteral("Display limit must be positive");
            return std::nullopt;
        }
        auto series = m_loader(provider, instrument, barInterval, errorMessage);
        if (!series) {
            if (errorMessage && errorMessage->isEmpty())
                *errorMessage = QStringLiteral("No stored market series matches this query");
            return std::nullopt;
        }
        MarketSeriesReadModel model{std::move(*series), false};
        if (model.series.bars.size() > displayLimit) {
            const qsizetype excess = model.series.bars.size() - displayLimit;
            model.series.bars.remove(0, excess);
            model.displayTruncated = true;
        }
        if (errorMessage) errorMessage->clear();
        return model;
    }

private:
    Loader m_loader;
};

class EconomicDataExplorerQuery final {
public:
    using Loader = std::function<std::optional<oqt::data::CanonicalDataset>(
        const QString&, const QString&, QString*)>;

    explicit EconomicDataExplorerQuery(Loader loader) : m_loader(std::move(loader)) {}

    [[nodiscard]] std::optional<EconomicDatasetReadModel> execute(
        const QString& providerId, const QString& datasetId,
        QString* errorMessage = nullptr, qsizetype displayLimit = 1000) const {
        const QString provider = providerId.trimmed();
        const QString dataset = datasetId.trimmed();
        if (provider.isEmpty() || dataset.isEmpty()) {
            if (errorMessage) *errorMessage = QStringLiteral("Provider and dataset are required");
            return std::nullopt;
        }
        if (!m_loader) {
            if (errorMessage) *errorMessage = QStringLiteral("Economic data query is unavailable");
            return std::nullopt;
        }
        if (displayLimit <= 0) {
            if (errorMessage) *errorMessage = QStringLiteral("Display limit must be positive");
            return std::nullopt;
        }
        auto loaded = m_loader(provider, dataset, errorMessage);
        if (!loaded) {
            if (errorMessage && errorMessage->isEmpty())
                *errorMessage = QStringLiteral("No stored economic dataset matches this query");
            return std::nullopt;
        }
        EconomicDatasetReadModel model{std::move(*loaded), false};
        if (model.dataset.observations.size() > displayLimit) {
            const qsizetype excess = model.dataset.observations.size() - displayLimit;
            model.dataset.observations.remove(0, excess);
            model.displayTruncated = true;
        }
        if (errorMessage) errorMessage->clear();
        return model;
    }

private:
    Loader m_loader;
};

} // namespace oqt::application
