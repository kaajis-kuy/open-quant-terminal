#pragma once

#include "core/providers/YahooClient.h"
#include "core/providers/YahooFinanceAdapter.h"

#include <QObject>

namespace oqt::providers {

class YahooPipeline final : public QObject {
    Q_OBJECT

public:
    explicit YahooPipeline(YahooClient* client = nullptr,
                           QObject* parent = nullptr);

    [[nodiscard]] bool isBusy() const;

    // Runs Yahoo chart fetch -> response decode -> canonical OHLCV normalization.
    [[nodiscard]] bool fetchChart(YahooChartRequest request,
                                  QString datasetId,
                                  QString requestId);

    void cancel();

signals:
    void finished(const QString& requestId,
                  const oqt::providers::AdapterResult& result);

private:
    void handleClientFinished(const QString& requestId,
                              const QByteArray& payload,
                              int httpStatus,
                              const QString& errorMessage);

    YahooClient* m_client = nullptr;
    YahooFinanceAdapter m_adapter;
    ProviderRequestContext m_context;
    bool m_busy = false;
};

} // namespace oqt::providers

Q_DECLARE_METATYPE(oqt::providers::AdapterResult)
