#pragma once

#include <QObject>
#include <QString>

namespace oqt::providers { struct AdapterResult; }

namespace oqt::application {

// UI-facing command and event boundary for asynchronous refresh tasks.
// Implementations own provider/service details; widgets only observe task outcomes.
class RefreshTaskController : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    ~RefreshTaskController() override = default;

    [[nodiscard]] virtual bool startMarketRefresh(
        const QString& providerSymbol, const QString& canonicalInstrumentId,
        const QString& interval, const QString& requestId) = 0;
    [[nodiscard]] virtual bool startEconomicRefresh(
        const QString& datasetId, const QString& requestId,
        QString* errorMessage = nullptr) = 0;
    virtual void cancelMarketRefresh() = 0;
    virtual void cancelEconomicRefresh() = 0;

signals:
    void marketRefreshFinished(const QString& requestId, bool persisted,
                               const QString& errorMessage,
                               const oqt::providers::AdapterResult& result);
    void economicRefreshFinished(const QString& requestId, bool persisted,
                                 const QString& errorMessage,
                                 const oqt::providers::AdapterResult& result);
};

} // namespace oqt::application
