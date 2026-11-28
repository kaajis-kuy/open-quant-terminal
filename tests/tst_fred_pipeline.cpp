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
    struct Reply {
        int status = 200;
        QByteArray body;
    };

    explicit PipelineMockServer(QObject* parent = nullptr) : QObject(parent) {
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* socket = server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    buffer[socket].append(socket->readAll());
                    if (!buffer[socket].contains("\r\n\r\n")) return;
                    buffer.remove(socket);
                    ++requests;

                    Reply reply;
                    if (!replies.isEmpty()) {
                        reply = replies.takeFirst();
                    } else {
                        reply.body = R"({"count":2,"offset":0,"limit":10,"observations":[{"date":"2020-01-01","value":"10.5"},{"date":"2020-04-01","value":"11.5"}]})";
                    }
                    const QByteArray reason = reply.status == 200 ? "OK" : "Mock Error";
                    const QByteArray response = "HTTP/1.1 "
                        + QByteArray::number(reply.status) + " " + reason
                        + "\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "
                        + QByteArray::number(reply.body.size()) + "\r\n\r\n" + reply.body;
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
    void enqueue(Reply reply) { replies.append(std::move(reply)); }
    int requests = 0;

private:
    QTcpServer server;
    QList<Reply> replies;
    QHash<QTcpSocket*, QByteArray> buffer;
};

QByteArray observationPage(int count, int offset, const QString& date,
                           const QString& value) {
    return QJsonDocument(QJsonObject{
        {QStringLiteral("count"), count},
        {QStringLiteral("offset"), offset},
        {QStringLiteral("limit"), 1},
        {QStringLiteral("observations"), QJsonArray{
            QJsonObject{{QStringLiteral("date"), date},
                        {QStringLiteral("value"), value}}
        }}
    }).toJson(QJsonDocument::Compact);
}

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
        QCOMPARE(result.rawResult.providerId, QStringLiteral("fred"));
        QCOMPARE(result.rawResult.operationId, QStringLiteral("observations"));
        QCOMPARE(result.rawResult.requestId, QStringLiteral("pipeline-001"));
        QVERIFY(result.rawResult.fetchedAt.isValid());
        QCOMPARE(result.rawResult.fetchedAt.timeSpec(), Qt::UTC);
        QVERIFY(result.dataset->provenance.retrievedAt.isValid());
        QCOMPARE(result.dataset->provenance.retrievedAt.timeSpec(), Qt::UTC);
        QCOMPARE(result.dataset->provenance.retrievedAt, result.rawResult.fetchedAt);
        QVERIFY(result.dataset->provenance.sourceUrl.startsWith(QStringLiteral("https://")));
        QCOMPARE(result.dataset->provenance.attributionId, QStringLiteral("FRED"));
        QVERIFY(result.rawResult.responseSchemaVersion == QStringLiteral("1.0"));
    }

    void preservesPartialResultWhenPageLimitIsReached() {
        PipelineMockServer server;
        QVERIFY(server.listen());
        server.enqueue({200, observationPage(2, 0, QStringLiteral("2020-01-01"),
                                              QStringLiteral("10.5"))});

        FredClient client(nullptr, nullptr, server.endpoint());
        FredPipeline pipeline(&client);
        QSignalSpy spy(&pipeline, &FredPipeline::finished);

        FredObservationRequest request;
        request.apiKey = QStringLiteral("pipeline-secret");
        request.seriesId = QStringLiteral("GDP");
        request.limit = 1;

        QVERIFY(pipeline.fetchObservations(request, QStringLiteral("fred.gdp"),
                                           QStringLiteral("pipeline-partial"), 1));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 5000);
        QCOMPARE(server.requests, 1);

        const AdapterResult result = qvariant_cast<AdapterResult>(spy.takeFirst().at(1));
        QVERIFY(result.isValid());
        QCOMPARE(result.responseStatus, ResponseStatus::Partial);
        QVERIFY(result.dataset.has_value());
        QCOMPARE(result.dataset->observations.size(), qsizetype(1));
        QVERIFY(result.dataset->quality.isPartial);
        QVERIFY(result.dataset->quality.isTruncated);
        QCOMPARE(result.dataset->quality.nextPageCursor, QStringLiteral("offset=1"));
    }

    void preservesRowsWhenLaterPageFails() {
        PipelineMockServer server;
        QVERIFY(server.listen());
        server.enqueue({200, observationPage(2, 0, QStringLiteral("2020-01-01"),
                                              QStringLiteral("10.5"))});
        server.enqueue({400, R"({"error_message":"invalid second-page request"})"});

        FredClient client(nullptr, nullptr, server.endpoint());
        FredPipeline pipeline(&client);
        QSignalSpy spy(&pipeline, &FredPipeline::finished);

        FredObservationRequest request;
        request.apiKey = QStringLiteral("pipeline-secret");
        request.seriesId = QStringLiteral("GDP");
        request.limit = 1;

        QVERIFY(pipeline.fetchObservations(request, QStringLiteral("fred.gdp"),
                                           QStringLiteral("pipeline-page-error"), 3));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 5000);
        QCOMPARE(server.requests, 2);

        const AdapterResult result = qvariant_cast<AdapterResult>(spy.takeFirst().at(1));
        QVERIFY(result.isValid());
        QCOMPARE(result.responseStatus, ResponseStatus::Partial);
        QVERIFY(result.dataset.has_value());
        QCOMPARE(result.dataset->observations.size(), qsizetype(1));
        QVERIFY(result.dataset->quality.isPartial);
        QVERIFY(result.dataset->quality.isTruncated);
        QVERIFY(!result.dataset->quality.warnings.isEmpty());
        QVERIFY(result.dataset->quality.warnings.join("; ").contains(QStringLiteral("400")));
        QCOMPARE(result.dataset->quality.nextPageCursor, QStringLiteral("offset=1"));
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
