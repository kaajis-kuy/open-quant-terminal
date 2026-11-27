#include "core/providers/FredPipeline.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QtTest>

using namespace oqt::providers;

namespace {

class PipelineMockServer final : public QObject {
    Q_OBJECT
public:
    explicit PipelineMockServer(QObject* parent = nullptr) : QObject(parent) {
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* socket = server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    buffer[socket].append(socket->readAll());
                    if (!buffer[socket].contains("\r\n\r\n")) return;
                    buffer.remove(socket);
                    ++requests;
                    const QByteArray body = R"({"count":2,"offset":0,"limit":10,"observations":[{"date":"2020-01-01","value":"10.5"},{"date":"2020-04-01","value":"11.5"}]})";
                    const QByteArray response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "
                        + QByteArray::number(body.size()) + "\r\n\r\n" + body;
                    socket->write(response);
                    socket->disconnectFromHost();
                });
            }
        });
    }

    bool listen() { return server.listen(QHostAddress::LocalHost, 0); }
    QUrl endpoint() const {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/fred/series/observations")
                    .arg(server.serverPort()));
    }
    int requests = 0;

private:
    QTcpServer server;
    QHash<QTcpSocket*, QByteArray> buffer;
};

} // namespace

class FredPipelineTest final : public QObject {
    Q_OBJECT

private slots:
    void fetchesAndNormalizesEndToEnd() {
        PipelineMockServer server;
        QVERIFY(server.listen());

        FredClient client(nullptr, nullptr, server.endpoint());
        FredPipeline pipeline(&client);
        QSignalSpy spy(&pipeline, &FredPipeline::finished);

        FredObservationRequest request;
        request.apiKey = QStringLiteral("pipeline-secret");
        request.seriesId = QStringLiteral("GDP");
        request.limit = 10;

        QVERIFY(pipeline.fetchObservations(request, QStringLiteral("fred.gdp"),
                                           QStringLiteral("pipeline-001"), 2));
        QVERIFY(pipeline.isBusy());
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 5000);
        QVERIFY(!pipeline.isBusy());
        QCOMPARE(server.requests, 1);

        const auto args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("pipeline-001"));
        const AdapterResult result = qvariant_cast<AdapterResult>(args.at(1));
        QVERIFY2(result.errors.isEmpty(), qPrintable(result.errors.join("; ")));
        QVERIFY(result.isValid());
        QCOMPARE(result.responseStatus, ResponseStatus::Ok);
        QVERIFY(result.dataset.has_value());
        QCOMPARE(result.dataset->providerId, QStringLiteral("fred"));
        QCOMPARE(result.dataset->datasetId, QStringLiteral("fred.gdp"));
        QCOMPARE(result.dataset->observations.size(), qsizetype(2));
        QCOMPARE(*result.dataset->observations.at(1).value, 11.5);
        QVERIFY(!result.rawResult.redactedRequestArguments.join(";")
                     .contains(QStringLiteral("pipeline-secret")));
        QVERIFY(result.rawResult.rawPayload.isObject());
    }

    void rejectsInvalidRequestBeforeStarting() {
        FredClient client;
        FredPipeline pipeline(&client);
        FredObservationRequest request;
        request.apiKey = QStringLiteral("key");
        request.seriesId = QStringLiteral("GDP");

        QVERIFY(!pipeline.fetchObservations(request, QString(), QStringLiteral("id")));
        QVERIFY(!pipeline.fetchObservations(request, QStringLiteral("dataset"), QString()));
        QVERIFY(!pipeline.isBusy());
    }
};

QTEST_GUILESS_MAIN(FredPipelineTest)

#include "tst_fred_pipeline.moc"
