#include "core/providers/YahooPipeline.h"

#include <QHash>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

using namespace oqt::providers;

namespace {

class YahooPipelineMockServer final : public QObject {
    Q_OBJECT
public:
    explicit YahooPipelineMockServer(QObject* parent = nullptr) : QObject(parent) {
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* socket = server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    buffers[socket].append(socket->readAll());
                    if (!buffers[socket].contains("\r\n\r\n")) return;
                    buffers.remove(socket);
                    ++requests;
                    const QByteArray body = responseBody;
                    const QByteArray reason = statusCode == 200 ? "OK" : "Mock Error";
                    const QByteArray response = "HTTP/1.1 "
                        + QByteArray::number(statusCode) + " " + reason
                        + "\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "
                        + QByteArray::number(body.size()) + "\r\n\r\n" + body;
                    socket->write(response);
                    socket->disconnectFromHost();
                });
            }
        });
    }

    bool listen() { return server.listen(QHostAddress::LocalHost, 0); }
    QUrl endpoint() const {
        return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()));
    }

    int statusCode = 200;
    int requests = 0;
    QByteArray responseBody = R"({
        "chart": {
            "result": [{
                "meta": {"symbol":"AAPL","currency":"USD"},
                "timestamp":[1704067200,1704153600],
                "indicators": {
                    "quote":[{"open":[100,101],"high":[105,106],"low":[99,100],"close":[104,105],"volume":[1000,1200]}],
                    "adjclose":[{"adjclose":[103,104]}]
                }
            }],
            "error": null
        }
    })";

private:
    QTcpServer server;
    QHash<QTcpSocket*, QByteArray> buffers;
};

} // namespace

class YahooPipelineTest final : public QObject {
    Q_OBJECT

private slots:
    void fetchesAndNormalizesOhlcvEndToEnd() {
        YahooPipelineMockServer server;
        QVERIFY(server.listen());

        YahooClient client(nullptr, nullptr, server.endpoint());
        YahooPipeline pipeline(&client);
        QSignalSpy spy(&pipeline, &YahooPipeline::finished);

        YahooChartRequest request;
        request.symbol = QStringLiteral("AAPL");
        request.interval = QStringLiteral("1d");
        request.range = QStringLiteral("1mo");

        QVERIFY(pipeline.fetchChart(request, QStringLiteral("equity.aapl"),
                                    QStringLiteral("yahoo-001")));
        QVERIFY(pipeline.isBusy());
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 5000);
        QVERIFY(!pipeline.isBusy());
        QCOMPARE(server.requests, 1);

        const auto args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("yahoo-001"));
        const AdapterResult result = qvariant_cast<AdapterResult>(args.at(1));
        QVERIFY2(result.errors.isEmpty(), qPrintable(result.errors.join("; ")));
        QVERIFY(result.isValid());
        QCOMPARE(result.responseStatus, ResponseStatus::Ok);
        QVERIFY(result.marketSeries.has_value());
        QCOMPARE(result.marketSeries->providerId, QStringLiteral("yahoo_finance"));
        QCOMPARE(result.marketSeries->instrumentId, QStringLiteral("equity.aapl"));
        QCOMPARE(result.marketSeries->providerSymbol, QStringLiteral("AAPL"));
        QCOMPARE(result.marketSeries->currency, QStringLiteral("USD"));
        QCOMPARE(result.marketSeries->bars.size(), qsizetype(2));
        QCOMPARE(result.marketSeries->bars.at(0).timestamp.timeSpec(), Qt::UTC);
        QCOMPARE(result.marketSeries->bars.at(1).close, 105.0);
        QCOMPARE(*result.marketSeries->bars.at(0).volume, 1000.0);
        QCOMPARE(*result.marketSeries->bars.at(0).adjustedClose, 103.0);
        QVERIFY(result.rawResult.rawPayload.isObject());
        QCOMPARE(result.rawResult.providerId, QStringLiteral("yahoo_finance"));
        QCOMPARE(result.rawResult.operationId, QStringLiteral("historical_ohlcv"));
        QCOMPARE(result.rawResult.requestId, QStringLiteral("yahoo-001"));
        QVERIFY(result.rawResult.fetchedAt.isValid());
        QCOMPARE(result.rawResult.fetchedAt.timeSpec(), Qt::UTC);
        QVERIFY(result.marketSeries->provenance.retrievedAt.isValid());
        QCOMPARE(result.marketSeries->provenance.retrievedAt.timeSpec(), Qt::UTC);
        QCOMPARE(result.marketSeries->provenance.retrievedAt, result.rawResult.fetchedAt);
        QVERIFY(result.marketSeries->provenance.sourceUrl.startsWith(QStringLiteral("https://")));
        QCOMPARE(result.marketSeries->provenance.attributionId, QStringLiteral("Yahoo Finance"));
        QCOMPARE(result.rawResult.responseSchemaVersion, QStringLiteral("1.0"));
        QVERIFY(result.rawResult.redactedRequestArguments.join(";")
                    .contains(QStringLiteral("symbol=AAPL")));
    }

    void turnsHttpErrorsIntoFailedAdapterResults() {
        YahooPipelineMockServer server;
        QVERIFY(server.listen());
        server.statusCode = 403;
        server.responseBody = R"({"chart":{"result":null,"error":{"code":"Forbidden","description":"blocked"}}})";

        YahooClient client(nullptr, nullptr, server.endpoint());
        YahooPipeline pipeline(&client);
        QSignalSpy spy(&pipeline, &YahooPipeline::finished);

        YahooChartRequest request;
        request.symbol = QStringLiteral("AAPL");
        QVERIFY(pipeline.fetchChart(request, QStringLiteral("equity.aapl"),
                                    QStringLiteral("yahoo-http-error")));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 5000);

        const AdapterResult result = qvariant_cast<AdapterResult>(spy.takeFirst().at(1));
        QCOMPARE(result.responseStatus, ResponseStatus::Failed);
        QVERIFY(!result.errors.isEmpty());
        QVERIFY(result.errors.join("; ").contains(QStringLiteral("HTTP 403")));
        QVERIFY(!result.isValid());
    }

    void rejectsMalformedJsonResponse() {
        YahooPipelineMockServer server;
        QVERIFY(server.listen());
        server.responseBody = QByteArrayLiteral("{not-json");

        YahooClient client(nullptr, nullptr, server.endpoint());
        YahooPipeline pipeline(&client);
        QSignalSpy spy(&pipeline, &YahooPipeline::finished);

        YahooChartRequest request;
        request.symbol = QStringLiteral("AAPL");
        QVERIFY(pipeline.fetchChart(request, QStringLiteral("equity.aapl"),
                                    QStringLiteral("yahoo-invalid-json")));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 5000);

        const AdapterResult result = qvariant_cast<AdapterResult>(spy.takeFirst().at(1));
        QCOMPARE(result.responseStatus, ResponseStatus::Failed);
        QVERIFY(!result.errors.isEmpty());
        QVERIFY(!result.isValid());
    }

    void rejectsYahooChartApiErrorOnHttpSuccess() {
        YahooPipelineMockServer server;
        QVERIFY(server.listen());
        server.responseBody = R"({"chart":{"result":null,"error":{"code":"Not Found","description":"No data found"}}})";

        YahooClient client(nullptr, nullptr, server.endpoint());
        YahooPipeline pipeline(&client);
        QSignalSpy spy(&pipeline, &YahooPipeline::finished);

        YahooChartRequest request;
        request.symbol = QStringLiteral("AAPL");
        QVERIFY(pipeline.fetchChart(request, QStringLiteral("equity.aapl"),
                                    QStringLiteral("yahoo-chart-error")));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 5000);

        const AdapterResult result = qvariant_cast<AdapterResult>(spy.takeFirst().at(1));
        QCOMPARE(result.responseStatus, ResponseStatus::Failed);
        QVERIFY(result.errors.join("; ").contains(QStringLiteral("No data found")));
        QVERIFY(!result.isValid());
    }

    void preservesPartialSeriesFromMalformedCandles() {
        YahooPipelineMockServer server;
        QVERIFY(server.listen());
        server.responseBody = R"({
            "chart":{"result":[{
                "meta":{"symbol":"AAPL","currency":"USD"},
                "timestamp":[1704067200,1704153600],
                "indicators":{"quote":[{
                    "open":[100,null],"high":[105,null],
                    "low":[99,null],"close":[104,null]
                }]}
            }],"error":null}
        })";

        YahooClient client(nullptr, nullptr, server.endpoint());
        YahooPipeline pipeline(&client);
        QSignalSpy spy(&pipeline, &YahooPipeline::finished);

        YahooChartRequest request;
        request.symbol = QStringLiteral("AAPL");
        QVERIFY(pipeline.fetchChart(request, QStringLiteral("equity.aapl"),
                                    QStringLiteral("yahoo-partial")));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 5000);

        const AdapterResult result = qvariant_cast<AdapterResult>(spy.takeFirst().at(1));
        QVERIFY2(result.errors.isEmpty(), qPrintable(result.errors.join("; ")));
        QVERIFY(result.isValid());
        QCOMPARE(result.responseStatus, ResponseStatus::Ok);
        QVERIFY(result.marketSeries.has_value());
        QCOMPARE(result.marketSeries->bars.size(), qsizetype(1));
        QCOMPARE(result.marketSeries->quality.status, oqt::data::DataStatus::Partial);
        QVERIFY(result.marketSeries->quality.isPartial);
    }

    void handlesExternalClientDestructionSafely() {
        auto* client = new YahooClient;
        YahooPipeline pipeline(client);
        delete client;

        YahooChartRequest request;
        request.symbol = QStringLiteral("AAPL");
        QVERIFY(!pipeline.fetchChart(request, QStringLiteral("equity.aapl"),
                                     QStringLiteral("yahoo-destroyed-client")));
        QVERIFY(!pipeline.isBusy());
    }

    void rejectsInvalidRequestsBeforeStarting() {
        YahooClient client;
        YahooPipeline pipeline(&client);
        YahooChartRequest request;
        request.symbol = QStringLiteral("AAPL");

        QVERIFY(!pipeline.fetchChart(request, {}, QStringLiteral("id")));
        QVERIFY(!pipeline.fetchChart(request, QStringLiteral("dataset"), {}));
        QVERIFY(!pipeline.isBusy());
    }

    void cancellationCompletesOnce() {
        YahooPipelineMockServer server;
        QVERIFY(server.listen());

        YahooClient client(nullptr, nullptr, server.endpoint());
        YahooPipeline pipeline(&client);
        QSignalSpy spy(&pipeline, &YahooPipeline::finished);

        YahooChartRequest request;
        request.symbol = QStringLiteral("AAPL");
        QVERIFY(pipeline.fetchChart(request, QStringLiteral("equity.aapl"),
                                    QStringLiteral("yahoo-cancel")));
        pipeline.cancel();

        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 3000);
        QVERIFY(!pipeline.isBusy());
        QCOMPARE(spy.first().at(0).toString(), QStringLiteral("yahoo-cancel"));
        QTest::qWait(100);
        QCOMPARE(spy.size(), 1);
        const AdapterResult result = qvariant_cast<AdapterResult>(spy.first().at(1));
        QCOMPARE(result.responseStatus, ResponseStatus::Failed);
        QVERIFY(result.errors.join("; ").contains(QStringLiteral("cancelled")));
    }
};

QTEST_GUILESS_MAIN(YahooPipelineTest)

#include "tst_yahoo_pipeline.moc"
