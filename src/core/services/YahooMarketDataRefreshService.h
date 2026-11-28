#pragma once

#include "core/providers/YahooPipeline.h"
#include "core/services/MarketDataIngestionService.h"

#include <QObject>
#include <QString>

namespace oqt::services {

// Coordinates Yahoo OHLCV retrieval with canonical SQLite persistence.
class YahooMarketDataRefreshService final : public QObject {
    Q_OBJECT

public:
    explicit YahooMarketDataRefreshService(
        oqt::providers::YahooPipeline& pipeline,
        MarketDataIngestionService& ingestion,
        QObject* parent = nullptr);

    [[nodiscard]] bool isBusy() const;

    // Starts an asynchronous fetch. Completion is reported after persistence.
    [[nodiscard]] bool refresh(
        oqt::providers::YahooChartRequest request,
        QString instrumentId,
        QString requestId,
        MarketDataWriteMode writeMode = MarketDataWriteMode::Upsert);

signals:
    void finished(const QString& requestId,
                  bool persisted,
                  const QString& errorMessage,
                  const oqt::providers::AdapterResult& result);

private:
    void handlePipelineFinished(
        const QString& requestId,
        const oqt::providers::AdapterResult& result);

    oqt::providers::YahooPipeline& m_pipeline;
    MarketDataIngestionService& m_ingestion;
    QString m_activeRequestId;
    MarketDataWriteMode m_writeMode = MarketDataWriteMode::Upsert;
};

} // namespace oqt::services
