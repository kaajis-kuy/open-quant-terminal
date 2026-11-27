#include "core/providers/FredClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QtTest>

using namespace oqt::providers;

namespace {

class MockFredServer final : public QObject {
    Q_OBJECT
public:
    struct Reply {
        int status = 200;
        QByteArray body;
        int delayMs = 0;
        QByteArray retryAfter;
    };

    explicit MockFredServer(QObject* parent = nullptr) : QObject(parent) {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* socket = m_server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    m_requestBuffer[socket].append(socket->readAll());
                    if (!m_requestBuffer[socket].contains("\r\n\r\n")) {
                        return;
                    }
                    const QByteArray request = m_requestBuffer.take(socket);
                    const QByteArray firstLine = request.left(request.indexOf("\r\n"));
                    const QList<QByteArray> parts = firstLine.split(' ');
                    m_paths.append(parts.size() > 1 ? parts.at(1) : QByteArray{});
                    Reply reply;
                    if (!m_replies.isEmpty()) {
                        reply = m_replies.takeFirst();
                    } else {
                        reply.body = R"({"count":0,"offset":0,"observations":[]})";
                    }
                    QPointer<QTcpSocket> guarded(socket);
                    QTimer::singleShot(reply.delayMs, this, [this, guarded, reply] {
                        if (!guarded || guarded->state() == QAbstractSocket::UnconnectedState) {
                            return;
                        }
                        QByteArray response = "HTTP/1.1 "
                            + QByteArray::number(reply.status) + " "
                            + (reply.status == 200 ? "OK" : "Mock Error")
                            + "\r\nContent-Type: application/json\r\nConnection: close\r\n";
                        if (!reply.retryAfter.isEmpty()) {
                            response += "Retry-After: " + reply.retryAfter + "\r\n";
                        }
                        response += "Content-Length: " + QByteArray::number(reply.body.size())
                            + "\r\n\r\n" + reply.body;
                        guarded->write(response);
                        guarded->disconnectFromHost();
                    });
                });
            }
        });
    }

    bool listen() { return m_server.listen(QHostAddress::LocalHost, 0); }
    QUrl endpoint() const {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/fred/series/observations")
                    .arg(m_server.serverPort()));
    }
    void enqueue(Reply reply) { m_replies.append(std::move(reply)); }
    int requestCount() const { return m_paths.size(); }
    QByteArray pathAt(int index) const { return m_paths.value(index); }

private:
    QTcpServer m_server;
    QList<Reply> m_replies;
    QList<QByteArray> m_paths;
    QHash<QTcpSocket*, QByteArray> m_requestBuffer;
};

QByteArray page(int count, int offset, const QList<QPair<QString, QString>>& rows) {
    QJsonArray observations;
    for (const auto& row : rows) {
        observations.append(QJsonObject{
            {QStringLiteral("date"), row.first},
            {QStringLiteral("value"), row.second}
        });
    }
    return QJsonDocument(QJsonObject{
        {QStringLiteral("count"), count},
        {QStringLiteral("offset"), offset},
        {QStringLiteral("limit"), 1},
        {QStringLiteral("observations"), observations}
    }).toJson(QJsonDocument::Compact);
}

FredObservationRequest request() {
    FredObservationRequest value;
    value.apiKey = QStringLiteral("mock-secret");
    value.seriesId = QStringLiteral("GDP");
    value.limit = 1;
    return value;
}

} // namespace

class FredClientTest final : public QObject {
    Q_OBJECT

private slots:
    void rejectsMissingCredentials() {
        FredClient client;
        FredObservationRequest input;
        input.seriesId = "GDP";
        QVERIFY(!client.fetchObservations(input, QStringLiteral("request-1")));
        QVERIFY(!client.isBusy());
    }

    void rejectsInvalidPageLimitWithoutStartingNetworkRequest() {
        FredClient client;
        auto input = request();
        QVERIFY(!client.fetchObservations(input, QStringLiteral("request-2"), 0));
        QVERIFY(!client.isBusy());
        QVERIFY(!client.fetchObservations(input, QStringLiteral("request-3"), 101));
        QVERIFY(!client.isBusy());
    }

    void rejectsEmptyRequestId() {
        FredClient client;
        QVERIFY(!client.fetchObservations(request(), QString()));
        QVERIFY(!client.isBusy());
    }

    void fetchesMultiplePagesSequentially() {
        MockFredServer server;
        QVERIFY(server.listen());
        server.enqueue({200, page(2, 0, {{QStringLiteral("2020-01-01"), QStringLiteral("1")}})});
        server.enqueue({200, page(2, 1, {{QStringLiteral("2020-04-01"), QStringLiteral("2")}})});

        FredClient client(nullptr, nullptr, server.endpoint());
        QSignalSpy spy(&client, &FredClient::finished);
        QVERIFY(client.fetchObservations(request(), QStringLiteral("pagination"), 5));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 5000);
        QCOMPARE(server.requestCount(), 2);
        QVERIFY(server.pathAt(0).contains("offset=0"));
        QVERIFY(server.pathAt(1).contains("offset=1"));
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("pagination"));
        QCOMPARE(args.at(2).toInt(), 2);
        QVERIFY(args.at(3).toString().isEmpty());
        const QJsonObject output = QJsonDocument::fromJson(args.at(1).toByteArray()).object();
        const QJsonArray observations = output.value(QStringLiteral("observations")).toArray();
        QCOMPARE(observations.size(), 2);
        QVERIFY(observations.at(0).isObject());
        QVERIFY(observations.at(1).isObject());
        QVERIFY(!output.value(QStringLiteral("truncated")).toBool());
    }

    void respectsMaximumPageCountAndReturnsCursor() {
        MockFredServer server;
        QVERIFY(server.listen());
        server.enqueue({200, page(3, 0, {{QStringLiteral("2020-01-01"), QStringLiteral("1")}})});
        FredClient client(nullptr, nullptr, server.endpoint());
        QSignalSpy spy(&client, &FredClient::finished);
        QVERIFY(client.fetchObservations(request(), QStringLiteral("page-cap"), 1));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 5000);
        QCOMPARE(server.requestCount(), 1);
        const auto args = spy.takeFirst();
        const QJsonObject output = QJsonDocument::fromJson(args.at(1).toByteArray()).object();
        QVERIFY(output.value(QStringLiteral("truncated")).toBool());
        QCOMPARE(output.value(QStringLiteral("next_cursor")).toString(), QStringLiteral("offset=1"));
    }

    void retriesRateLimitAndHonorsRetryAfter() {
        MockFredServer server;
        QVERIFY(server.listen());
        server.enqueue({429, R"({"error":"rate limited"})", 0, QByteArrayLiteral("0")});
        server.enqueue({200, page(1, 0, {{QStringLiteral("2020-01-01"), QStringLiteral("1")}})});
        FredClient client(nullptr, nullptr, server.endpoint());
        QSignalSpy spy(&client, &FredClient::finished);
        QVERIFY(client.fetchObservations(request(), QStringLiteral("retry"), 2));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 7000);
        QCOMPARE(server.requestCount(), 2);
        QVERIFY(spy.takeFirst().at(3).toString().isEmpty());
    }


    void parsesHttpDateRetryAfter() {
        MockFredServer server;
        QVERIFY(server.listen());
        const QByteArray retryAt = QDateTime::currentDateTimeUtc()
            .addSecs(1).toString(Qt::RFC2822Date).toLatin1();
        server.enqueue({429, R"({"error":"rate limited"})", 0, retryAt});
        server.enqueue({200, page(1, 0, {{QStringLiteral("2020-01-01"), QStringLiteral("1")}})});
        FredClient client(nullptr, nullptr, server.endpoint());
        QSignalSpy spy(&client, &FredClient::finished);
        QVERIFY(client.fetchObservations(request(), QStringLiteral("retry-http-date"), 1));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 5000);
        QCOMPARE(server.requestCount(), 2);
        QVERIFY(spy.takeFirst().at(3).toString().isEmpty());
    }

    void cancellationThenRestartIgnoresStaleReply() {
        MockFredServer server;
        QVERIFY(server.listen());
        server.enqueue({200, page(1, 0, {{QStringLiteral("2020-01-01"), QStringLiteral("stale")}}), 500});
        server.enqueue({200, page(1, 0, {{QStringLiteral("2020-04-01"), QStringLiteral("fresh")}})});
        FredClient client(nullptr, nullptr, server.endpoint());
        QSignalSpy spy(&client, &FredClient::finished);

        QVERIFY(client.fetchObservations(request(), QStringLiteral("cancel-old"), 1));
        client.cancel();
        QVERIFY(client.fetchObservations(request(), QStringLiteral("new-request"), 1));

        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 2, 5000);
        const auto cancelled = spy.takeFirst();
        QCOMPARE(cancelled.at(0).toString(), QStringLiteral("cancel-old"));
        QCOMPARE(cancelled.at(3).toString(), QStringLiteral("FRED request cancelled"));

        const auto completed = spy.takeFirst();
        QCOMPARE(completed.at(0).toString(), QStringLiteral("new-request"));
        QVERIFY(completed.at(3).toString().isEmpty());
        const QJsonObject output = QJsonDocument::fromJson(completed.at(1).toByteArray()).object();
        const QJsonArray rows = output.value(QStringLiteral("observations")).toArray();
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.at(0).toObject().value(QStringLiteral("value")).toString(),
                 QStringLiteral("fresh"));
        QCOMPARE(server.requestCount(), 2);
    }

    void reportsHttpClientError() {
        MockFredServer server;
        QVERIFY(server.listen());
        server.enqueue({400, R"({"error_message":"bad request"})"});
        FredClient client(nullptr, nullptr, server.endpoint());
        QSignalSpy spy(&client, &FredClient::finished);
        QVERIFY(client.fetchObservations(request(), QStringLiteral("http-error"), 1));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 5000);
        QCOMPARE(server.requestCount(), 1);
        QVERIFY(spy.takeFirst().at(3).toString().contains(QStringLiteral("400")));
    }

    void reportsInvalidJson() {
        MockFredServer server;
        QVERIFY(server.listen());
        server.enqueue({200, "{not-json"});
        FredClient client(nullptr, nullptr, server.endpoint());
        QSignalSpy spy(&client, &FredClient::finished);
        QVERIFY(client.fetchObservations(request(), QStringLiteral("invalid-json"), 1));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 5000);
        QVERIFY(spy.takeFirst().at(3).toString().contains(QStringLiteral("JSON")));
    }

    void cancellationCompletesOnce() {
        MockFredServer server;
        QVERIFY(server.listen());
        server.enqueue({200, page(1, 0, {{QStringLiteral("2020-01-01"), QStringLiteral("1")}}), 2000});
        FredClient client(nullptr, nullptr, server.endpoint());
        QSignalSpy spy(&client, &FredClient::finished);
        QVERIFY(client.fetchObservations(request(), QStringLiteral("cancel"), 1));
        QVERIFY(client.isBusy());
        client.cancel();
        QVERIFY(!client.isBusy());
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 1000);
        QCOMPARE(spy.takeFirst().at(3).toString(), QStringLiteral("FRED request cancelled"));
        QTest::qWait(2200);
        QCOMPARE(spy.size(), 0);
    }
};

QTEST_GUILESS_MAIN(FredClientTest)

#include "tst_fred_client.moc"
