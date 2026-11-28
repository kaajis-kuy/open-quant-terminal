#include "core/providers/YahooClient.h"

#include <QHash>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QtTest>

using namespace oqt::providers;

namespace {
class MockYahooServer final : public QObject {
    Q_OBJECT
public:
    struct Reply { int status = 200; QByteArray body = R"({"chart":{"result":[],"error":null}})"; int delayMs = 0; };
    explicit MockYahooServer(QObject* parent = nullptr) : QObject(parent) {
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* socket = server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    buffers[socket].append(socket->readAll());
                    if (!buffers.value(socket).contains("\r\n\r\n")) return;
                    buffers.remove(socket);
                    ++requests;
                    const Reply reply = replies.isEmpty() ? Reply{} : replies.takeFirst();
                    QPointer<QTcpSocket> guarded(socket);
                    QTimer::singleShot(reply.delayMs, this, [guarded, reply] {
                        if (!guarded) return;
                        const QByteArray reason = reply.status == 200 ? "OK" : "Mock Error";
                        const QByteArray response = "HTTP/1.1 " + QByteArray::number(reply.status)
                            + " " + reason + "\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "
                            + QByteArray::number(reply.body.size()) + "\r\n\r\n" + reply.body;
                        guarded->write(response);
                        guarded->disconnectFromHost();
                    });
                });
            }
        });
    }
    bool listen() { return server.listen(QHostAddress::LocalHost, 0); }
    QUrl endpoint() const { return QUrl(QStringLiteral("http://127.0.0.1:%1/v8/finance/chart").arg(server.serverPort())); }
    void enqueue(Reply reply) { replies.append(std::move(reply)); }
    int requests = 0;
private:
    QTcpServer server;
    QList<Reply> replies;
    QHash<QTcpSocket*, QByteArray> buffers;
};
YahooChartRequest chartRequest() {
    YahooChartRequest request;
    request.symbol = QStringLiteral("AAPL");
    request.range = QStringLiteral("1mo");
    return request;
}
}

class YahooClientTest final : public QObject {
    Q_OBJECT
private slots:
    void rejectsInvalidRequestAndEmptyId() {
        YahooClient client;
        QVERIFY(!client.fetchChart(chartRequest(), {}));
        auto invalid = chartRequest();
        invalid.symbol.clear();
        QVERIFY(!client.fetchChart(invalid, QStringLiteral("invalid")));
        QVERIFY(!client.isBusy());
    }

    void fetchesChartFromMockServer() {
        MockYahooServer server;
        QVERIFY(server.listen());
        server.enqueue({200, R"({"chart":{"result":[{"meta":{"symbol":"AAPL"}}],"error":null}})"});
        YahooClient client(nullptr, nullptr, server.endpoint());
        QSignalSpy spy(&client, &YahooClient::finished);
        QVERIFY(client.fetchChart(chartRequest(), QStringLiteral("success")));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 3000);
        QCOMPARE(server.requests, 1);
        const auto args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("success"));
        QCOMPARE(args.at(2).toInt(), 200);
        QVERIFY(args.at(3).toString().isEmpty());
        QVERIFY(args.at(1).toByteArray().contains("AAPL"));
        QVERIFY(!client.isBusy());
    }

    void reportsHttpError() {
        MockYahooServer server;
        QVERIFY(server.listen());
        server.enqueue({403, R"({"error":"forbidden"})"});
        YahooClient client(nullptr, nullptr, server.endpoint());
        QSignalSpy spy(&client, &YahooClient::finished);
        QVERIFY(client.fetchChart(chartRequest(), QStringLiteral("http-error")));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 3000);
        const auto args = spy.takeFirst();
        QCOMPARE(args.at(2).toInt(), 403);
        QVERIFY(args.at(3).toString().contains(QStringLiteral("403")));
    }

    void reportsTimeout() {
        MockYahooServer server;
        QVERIFY(server.listen());
        server.enqueue({200, R"({"chart":{"result":[],"error":null}})", 300});
        YahooClient client(nullptr, nullptr, server.endpoint(), 50);
        QSignalSpy spy(&client, &YahooClient::finished);
        QVERIFY(client.fetchChart(chartRequest(), QStringLiteral("timeout")));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 2000);
        QVERIFY(!spy.takeFirst().at(3).toString().isEmpty());
    }

    void cancellationCompletesOnce() {
        MockYahooServer server;
        QVERIFY(server.listen());
        server.enqueue({200, R"({"chart":{"result":[],"error":null}})", 500});
        YahooClient client(nullptr, nullptr, server.endpoint());
        QSignalSpy spy(&client, &YahooClient::finished);
        QVERIFY(client.fetchChart(chartRequest(), QStringLiteral("cancel")));
        QTRY_COMPARE_WITH_TIMEOUT(server.requests, 1, 1000);
        client.cancel();
        QVERIFY(!client.isBusy());
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 1000);
        QCOMPARE(spy.takeFirst().at(3).toString(), QStringLiteral("Yahoo Finance request cancelled"));
        QTest::qWait(600);
        QCOMPARE(spy.size(), 0);
    }
};

QTEST_GUILESS_MAIN(YahooClientTest)
#include "tst_yahoo_client.moc"
