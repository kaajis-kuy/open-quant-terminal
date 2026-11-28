#pragma once

#include "core/data/CanonicalData.h"
#include "core/data/MarketData.h"

#include <functional>
#include <limits>
#include <optional>
#include <utility>

namespace oqt::application {

template <typename T>
struct QueryPage {
    T data;
    bool hasMore = false;
    qint64 offset = 0;
    qsizetype pageSize = 0;
};

struct MarketSeriesReadModel {
    oqt::data::CanonicalMarketSeries series;
    bool displayTruncated = false;
    bool hasMore = false;
    qint64 nextOffset = 0;
    qsizetype pageSize = 0;
};

struct EconomicDatasetReadModel {
    oqt::data::CanonicalDataset dataset;
    bool displayTruncated = false;
    bool hasMore = false;
    qint64 nextOffset = 0;
    qsizetype pageSize = 0;
};

// Application-facing read boundary. Infrastructure is supplied as a callback
// by the composition root; feature UI never receives a concrete SQLite store.
class MarketExplorerQuery final {
public:
    using Loader = std::function<std::optional<QueryPage<oqt::data::CanonicalMarketSeries>>(
        const QString&, const QString&, const QString&, qsizetype, qint64, QString*)>;

    explicit MarketExplorerQuery(Loader loader) : m_loader(std::move(loader)) {}

    [[nodiscard]] std::optional<MarketSeriesReadModel> execute(
        const QString& providerId, const QString& instrumentId,
        const QString& interval, QString* errorMessage = nullptr,
        qsizetype pageSize = 1000, qint64 offset = 0) const {
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
        if (pageSize <= 0 || pageSize > 5000 || offset < 0
            || offset > std::numeric_limits<qint64>::max() - pageSize - 1) {
            if (errorMessage) *errorMessage = QStringLiteral("Page size must be between 1 and 5000 and offset cannot be negative");
            return std::nullopt;
        }
        auto page = m_loader(provider, instrument, barInterval, pageSize, offset, errorMessage);
        if (!page) {
            if (errorMessage && errorMessage->isEmpty())
                *errorMessage = QStringLiteral("No stored market series matches this query");
            return std::nullopt;
        }
        MarketSeriesReadModel model;
        model.series = std::move(page->data);
        model.displayTruncated = page->hasMore;
        model.hasMore = page->hasMore;
        model.nextOffset = offset + model.series.bars.size();
        model.pageSize = pageSize;
        if (errorMessage) errorMessage->clear();
        return model;
    }

private:
    Loader m_loader;
};

class EconomicDataExplorerQuery final {
public:
    using Loader = std::function<std::optional<QueryPage<oqt::data::CanonicalDataset>>(
        const QString&, const QString&, qsizetype, qint64, QString*)>;

    explicit EconomicDataExplorerQuery(Loader loader) : m_loader(std::move(loader)) {}

    [[nodiscard]] std::optional<EconomicDatasetReadModel> execute(
        const QString& providerId, const QString& datasetId,
        QString* errorMessage = nullptr, qsizetype pageSize = 1000,
        qint64 offset = 0) const {
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
        if (pageSize <= 0 || pageSize > 5000 || offset < 0) {
            if (errorMessage) *errorMessage = QStringLiteral("Page size must be between 1 and 5000 and offset cannot be negative");
            return std::nullopt;
        }
        auto page = m_loader(provider, dataset, pageSize, offset, errorMessage);
        if (!page) {
            if (errorMessage && errorMessage->isEmpty())
                *errorMessage = QStringLiteral("No stored economic dataset matches this query");
            return std::nullopt;
        }
        EconomicDatasetReadModel model;
        model.dataset = std::move(page->data);
        model.displayTruncated = page->hasMore;
        model.hasMore = page->hasMore;
        model.nextOffset = offset + model.dataset.observations.size();
        model.pageSize = pageSize;
        if (errorMessage) errorMessage->clear();
        return model;
    }

private:
    Loader m_loader;
};

} // namespace oqt::application
