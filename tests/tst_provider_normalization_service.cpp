#include "core/services/ProviderNormalizationService.h"

#include "core/providers/FredAdapter.h"

#include <QtTest>

using namespace oqt::providers;

class ProviderNormalizationServiceTest final : public QObject {
    Q_OBJECT

private slots:
    void normalizesRegisteredProviderOperation() {
        ProviderAdapterRegistry registry;
        QVERIFY(registry.registerAdapter(std::make_shared<FredAdapter>()));
        oqt::services::ProviderNormalizationService service(registry);

        ProviderRequestContext context;
        context.providerId = QStringLiteral("fred");
        context.operationId = QStringLiteral("observations");
        context.datasetId = QStringLiteral("gross_domestic_product");
        context.requestId = QStringLiteral("request-001");
        context.requestedAt = QDateTime::fromString(
            QStringLiteral("2026-01-01T00:00:00Z"), Qt::ISODate);
        context.redactedParameters.insert(QStringLiteral("series_id"),
                                          QStringLiteral("GDP"));

        ProviderResponse response;
        response.status = ResponseStatus::Ok;
        QJsonObject observation;
        observation.insert(QStringLiteral("date"), QStringLiteral("2025-01-01"));
        observation.insert(QStringLiteral("value"), QStringLiteral("123.45"));
        QJsonArray observations;
        observations.append(observation);
        QJsonObject payload;
        payload.insert(QStringLiteral("observations"), observations);
        payload.insert(QStringLiteral("count"), 1);
        payload.insert(QStringLiteral("offset"), 0);
        response.data = payload;

        const AdapterResult result = service.normalize(response, context);
        QVERIFY2(result.isValid(), qPrintable(result.errors.join(QStringLiteral("; "))));
        QVERIFY(result.dataset.has_value());
        QCOMPARE(result.rawResult.providerId, QStringLiteral("fred"));
        QCOMPARE(result.rawResult.operationId, QStringLiteral("observations"));
        QCOMPARE(result.rawResult.requestId, QStringLiteral("request-001"));
    }

    void rejectsUnknownProvider() {
        ProviderAdapterRegistry registry;
        oqt::services::ProviderNormalizationService service(registry);

        ProviderRequestContext context;
        context.providerId = QStringLiteral("unknown");
        context.operationId = QStringLiteral("observations");
        context.requestId = QStringLiteral("request-002");

        const AdapterResult result = service.normalize(ProviderResponse{}, context);
        QVERIFY(!result.isValid());
        QVERIFY(result.errors.join(QLatin1Char(' ')).contains(
            QStringLiteral("No adapter registered")));
        QCOMPARE(result.responseStatus, ResponseStatus::Failed);
    }

    void rejectsUnsupportedOperation() {
        ProviderAdapterRegistry registry;
        QVERIFY(registry.registerAdapter(std::make_shared<FredAdapter>()));
        oqt::services::ProviderNormalizationService service(registry);

        ProviderRequestContext context;
        context.providerId = QStringLiteral("fred");
        context.operationId = QStringLiteral("releases");
        context.requestId = QStringLiteral("request-003");

        const AdapterResult result = service.normalize(ProviderResponse{}, context);
        QVERIFY(!result.isValid());
        QVERIFY(result.errors.join(QLatin1Char(' ')).contains(
            QStringLiteral("does not support operation")));
    }

    void rejectsMissingProviderOrOperation() {
        ProviderAdapterRegistry registry;
        oqt::services::ProviderNormalizationService service(registry);

        ProviderRequestContext context;
        context.providerId = QStringLiteral("fred");
        context.requestId = QStringLiteral("request-004");
        const AdapterResult result = service.normalize(ProviderResponse{}, context);

        QVERIFY(!result.isValid());
        QVERIFY(result.errors.join(QLatin1Char(' ')).contains(
            QStringLiteral("Provider ID and operation ID are required")));
    }
};

QTEST_GUILESS_MAIN(ProviderNormalizationServiceTest)

#include "tst_provider_normalization_service.moc"
