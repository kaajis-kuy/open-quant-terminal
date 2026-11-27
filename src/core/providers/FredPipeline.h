#pragma once

#include "core/providers/FredAdapter.h"
#include "core/providers/FredClient.h"

#include <QObject>

namespace oqt::providers {

class FredPipeline final : public QObject {
    Q_OBJECT

public:
    explicit FredPipeline(FredClient* client = nullptr,
                          QObject* parent = nullptr);

    [[nodiscard]] bool isBusy() const;

    // Runs the complete FRED fetch -> response decode -> canonical normalization flow.
    // The API key remains inside FredClient and is never copied into request metadata.
    [[nodiscard]] bool fetchObservations(FredObservationRequest request,
                                         QString datasetId,
                                         QString requestId,
                                         int maxPages = 10);

    void cancel();

signals:
    void finished(const QString& requestId,
                  const oqt::providers::AdapterResult& result);

private:
    void handleClientFinished(const QString& requestId,
                              const QByteArray& aggregatedJson,
                              int pagesFetched,
                              const QString& errorMessage);

    FredClient* m_client = nullptr;
    FredAdapter m_adapter;
    ProviderRequestContext m_context;
    bool m_busy = false;
};

} // namespace oqt::providers

Q_DECLARE_METATYPE(oqt::providers::AdapterResult)
