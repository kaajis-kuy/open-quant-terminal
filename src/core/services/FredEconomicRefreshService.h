#pragma once

#include "core/providers/FredPipeline.h"
#include "core/services/EconomicDataIngestionService.h"

#include <QObject>
#include <QString>

namespace oqt::services {

// Coordinates FRED retrieval with canonical economic-data persistence.
class FredEconomicRefreshService final : public QObject {
    Q_OBJECT

public:
    explicit FredEconomicRefreshService(
        oqt::providers::FredPipeline& pipeline,
        EconomicDataIngestionService& ingestion,
        QObject* parent = nullptr);

    [[nodiscard]] bool isBusy() const;
    void cancel();

    [[nodiscard]] bool refresh(
        oqt::providers::FredObservationRequest request,
        QString datasetId,
        QString requestId,
        int maxPages = 10);

signals:
    void finished(const QString& requestId,
                  bool persisted,
                  const QString& errorMessage,
                  const oqt::providers::AdapterResult& result);

private:
    void handlePipelineFinished(
        const QString& requestId,
        const oqt::providers::AdapterResult& result);

    oqt::providers::FredPipeline& m_pipeline;
    EconomicDataIngestionService& m_ingestion;
    QString m_activeRequestId;
};

} // namespace oqt::services
