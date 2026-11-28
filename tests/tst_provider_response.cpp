#include "core/providers/ProviderResponse.h"

#include <QtTest>

using namespace oqt::providers;

class ProviderResponseTest final : public QObject {
    Q_OBJECT

private slots:
    void directArray_isAccepted() {
        const auto response = ProviderResponse::decode(R"([{"date":"2025-01-01","value":1}])");
        QCOMPARE(response.status, ResponseStatus::Ok);
        QVERIFY(response.hasData());
    }

    void emptyData_isClassifiedEmpty() {
        const auto response = ProviderResponse::decode(R"({"data":[]})");
        QCOMPARE(response.status, ResponseStatus::Empty);
        QVERIFY(!response.hasData());
    }

    void partialDataWithError_isNotDiscarded() {
        const auto response = ProviderResponse::decode(
            R"({"data":[{"value":1}],"error":"second page failed"})");
        QCOMPARE(response.status, ResponseStatus::Partial);
        QVERIFY(response.hasData());
        QCOMPARE(response.errorMessage, QStringLiteral("second page failed"));
    }

    void errorWithoutData_isFailed() {
        const auto response = ProviderResponse::decode(R"({"error":"invalid API key"})");
        QCOMPARE(response.status, ResponseStatus::Failed);
        QVERIFY(!response.hasData());
    }

    void truncatedPayload_isPartial() {
        const auto response = ProviderResponse::decode(
            R"({"data":[{"value":1}],"truncated":true,"next_cursor":"page-2"})");
        QCOMPARE(response.status, ResponseStatus::Partial);
        QCOMPARE(response.nextPageCursor, QStringLiteral("page-2"));
    }

    void directObject_isAccepted() {
        const auto response = ProviderResponse::decode(R"({"series_id":"GDPC1","count":1})");
        QCOMPARE(response.status, ResponseStatus::Ok);
        QVERIFY(response.data.isObject());
    }

    void malformedJson_isFailed() {
        const auto response = ProviderResponse::decode("{not-json");
        QCOMPARE(response.status, ResponseStatus::Failed);
        QVERIFY(!response.errorMessage.isEmpty());
    }
};

QTEST_GUILESS_MAIN(ProviderResponseTest)

#include "tst_provider_response.moc"
