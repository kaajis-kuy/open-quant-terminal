#include "core/services/YahooMarketDataRefreshService.h"

#include "core/providers/YahooClient.h"
#include "core/storage/SqliteMarketDataStore.h"

#include <QHash>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

using namespace oqt::providers;
using namespace oqt::services;

namespace {

class RefreshMockServer final : public QObject {
    Q_OBJECT
public:
    explicit RefreshMockServer(QObject* parent = nullptr) : QObject(parent) {
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* socket = server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    requestBuffer[socket].append(socket->readAll());
                    if (!requestBuffer[socket].contains("\r\n\r\n")) {
                        return;
                    }
                    requestBuffer.remove(socket);
                    ++requests;
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
    QByteArray body = R"({
        "chart":{"result":[{
            "meta":{"symbol":"AAPL","currency":"USD"},
            "timestamp":[1704067200,1704153600],
            "indicators":{"quote":[{
                "open":[100,101],"high":[105,106],
                "low":[99,100],"close":[104,105],
                "volume":[1000,1200]
            }]}
        }],"error":null}
    })";

private:
    QTcpServer server;
    QHash<QTcpSocket*, QByteArray> requestBuffer;
};

} // namespace

class YahooMarketDataRefreshServiceTest final : public QObject {
    Q_OBJECT

private slots:
    void fetchesNormalizesAndPersistsSeries() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString databasePath = directory.filePath(QStringLiteral("market-data.sqlite"));

        oqt::storage::SqliteMarketDataStore store(databasePath);
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));

        MarketDataIngestionService ingestion(store);
        RefreshMockServer server;
        QVERIFY(server.listen());

        YahooClient client(nullptr, nullptr, server.endpoint());
        YahooPipeline pipeline(&client);
        YahooMarketDataRefreshService refresh(pipeline, ingestion);
        QSignalSpy spy(&refresh, &YahooMarketDataRefreshService::finished);

        YahooChartRequest request;
        request.symbol = QStringLiteral("AAPL");
        request.interval = QStringLiteral("1d");
        request.range = QStringLiteral("1mo");

        QVERIFY(refresh.refresh(request, QStringLiteral("equity.aapl"),
                                QStringLiteral("refresh-001")));
        QVERIFY(refresh.isBusy());
        QVERIFY(!refresh.refresh(request, QStringLiteral("equity.aapl"),
                                 QStringLiteral("refresh-duplicate")));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 5000);
        QVERIFY(!refresh.isBusy());
        QCOMPARE(server.requests, 1);

        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("refresh-001"));
        QVERIFY(args.at(1).toBool());
        QVERIFY(args.at(2).toString().isEmpty());

        const AdapterResult result = qvariant_cast<AdapterResult>(args.at(3));
        QVERIFY(result.isValid());
        QVERIFY(result.marketSeries.has_value());
        QCOMPARE(result.marketSeries->bars.size(), qsizetype(2));

        const auto stored = store.loadSeries(
            QStringLiteral("yahoo_finance"),
            QStringLiteral("equity.aapl"),
            QStringLiteral("1d"),
            &error);
        QVERIFY2(stored.has_value(), qPrintable(error));
        QCOMPARE(stored->bars.size(), qsizetype(2));
        QCOMPARE(stored->bars.at(1).close, 105.0);
        QVERIFY(stored->validate().isEmpty());
    }

    void cancellationCompletesWithoutPersisting() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        oqt::storage::SqliteMarketDataStore store(
            directory.filePath(QStringLiteral("market-data.sqlite")));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));

        MarketDataIngestionService ingestion(store);
        RefreshMockServer server;
        QVERIFY(server.listen());

        YahooClient client(nullptr, nullptr, server.endpoint());
        YahooPipeline pipeline(&client);
        YahooMarketDataRefreshService refresh(pipeline, ingestion);
        QSignalSpy spy(&refresh, &YahooMarketDataRefreshService::finished);

        YahooChartRequest request;
        request.symbol = QStringLiteral("AAPL");
        QVERIFY(refresh.refresh(request, QStringLiteral("equity.aapl"),
                                QStringLiteral("refresh-cancelled")));
        QVERIFY(refresh.isBusy());
        refresh.cancel();

        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 5000);
        QVERIFY(!refresh.isBusy());
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("refresh-cancelled"));
        QVERIFY(!args.at(1).toBool());
        QVERIFY(args.at(2).toString().contains(QStringLiteral("invalid")));
        const AdapterResult result = qvariant_cast<AdapterResult>(args.at(3));
        QCOMPARE(result.responseStatus, ResponseStatus::Failed);
        QVERIFY(server.requests <= 1);

        const auto stored = store.loadSeries(
            QStringLiteral("yahoo_finance"),
            QStringLiteral("equity.aapl"),
            QStringLiteral("1d"),
            &error);
        QVERIFY(!stored.has_value());
    }

    void reportsProviderFailureWithoutPersisting() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        oqt::storage::SqliteMarketDataStore store(
            directory.filePath(QStringLiteral("market-data.sqlite")));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));

        MarketDataIngestionService ingestion(store);
        RefreshMockServer server;
        QVERIFY(server.listen());
        server.statusCode = 403;
        server.body = R"({"chart":{"result":null,"error":{"code":"Forbidden","description":"blocked"}}})";

        YahooClient client(nullptr, nullptr, server.endpoint());
        YahooPipeline pipeline(&client);
        YahooMarketDataRefreshService refresh(pipeline, ingestion);
        QSignalSpy spy(&refresh, &YahooMarketDataRefreshService::finished);

        YahooChartRequest request;
        request.symbol = QStringLiteral("AAPL");
        QVERIFY(refresh.refresh(request, QStringLiteral("equity.aapl"),
                                QStringLiteral("refresh-failed")));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 5000);

        const QList<QVariant> args = spy.takeFirst();
        QVERIFY(!args.at(1).toBool());
        QVERIFY(args.at(2).toString().contains(QStringLiteral("invalid")));
        const AdapterResult result = qvariant_cast<AdapterResult>(args.at(3));
        QCOMPARE(result.responseStatus, ResponseStatus::Failed);

        const auto stored = store.loadSeries(
            QStringLiteral("yahoo_finance"),
            QStringLiteral("equity.aapl"),
            QStringLiteral("1d"),
            &error);
        QVERIFY(!stored.has_value());
    }
};

QTEST_GUILESS_MAIN(YahooMarketDataRefreshServiceTest)

#include "tst_yahoo_market_data_refresh_service.moc"
