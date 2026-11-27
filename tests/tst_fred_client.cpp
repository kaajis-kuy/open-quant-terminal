#include "core/providers/FredClient.h"

#include <QtTest>

using namespace oqt::providers;

class FredClientTest final : public QObject {
    Q_OBJECT

private slots:
    void rejectsMissingCredentials() {
        FredClient client;
        FredObservationRequest request;
        request.seriesId = "GDP";
        QVERIFY(!client.fetchObservations(request, QStringLiteral("request-1")));
        QVERIFY(!client.isBusy());
    }

    void rejectsInvalidPageLimitWithoutStartingNetworkRequest() {
        FredClient client;
        FredObservationRequest request;
        request.apiKey = "test-secret";
        request.seriesId = "GDP";
        QVERIFY(!client.fetchObservations(request, QStringLiteral("request-2"), 0));
        QVERIFY(!client.isBusy());
        QVERIFY(!client.fetchObservations(request, QStringLiteral("request-3"), 101));
        QVERIFY(!client.isBusy());
    }

    void rejectsEmptyRequestId() {
        FredClient client;
        FredObservationRequest request;
        request.apiKey = "test-secret";
        request.seriesId = "GDP";
        QVERIFY(!client.fetchObservations(request, QString()));
        QVERIFY(!client.isBusy());
    }
};

QTEST_GUILESS_MAIN(FredClientTest)

#include "tst_fred_client.moc"
