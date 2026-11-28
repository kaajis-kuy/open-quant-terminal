#include "core/providers/FredPipeline.h"
#include "core/services/EconomicDataIngestionService.h"
#include "core/services/FredEconomicRefreshService.h"
#include "core/storage/SqliteCanonicalDatasetStore.h"

#include <QDir>
#include <QHash>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QtTest>

using namespace oqt;

class FredRefreshMockServer final : public QObject {
    Q_OBJECT
public:
    explicit FredRefreshMockServer(QObject* parent = nullptr) : QObject(parent) {
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* socket = server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    buffers[socket].append(socket->readAll());
                    if (!buffers[socket].contains("\r\n\r\n")) return;
                    buffers.remove(socket);
                    ++requestCount;
                    const QByteArray body = R"({"count":2,"offset":0,"limit":10,"observations":[{"date":"2024-01-01","value":"100.5"},{"date":"2024-02-01","value":"101.5"}]})";
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
    int requestCount = 0;
private:
    QTcpServer server;
    QHash<QTcpSocket*, QByteArray> buffers;
};

class FredEconomicRefreshServiceTest final : public QObject {
    Q_OBJECT
private slots:
    void fetchesNormalizesAndPersistsDataset() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        FredRefreshMockServer server;
        QVERIFY(server.listen());

        storage::SqliteCanonicalDatasetStore store(
            QDir(directory.path()).filePath("economic.sqlite"));
        QString error;
        QVERIFY2(store.open(&error), qPrintable(error));
        services::EconomicDataIngestionService ingestion(store);
        providers::FredClient client(nullptr, nullptr, server.endpoint());
        providers::FredPipeline pipeline(&client);
        services::FredEconomicRefreshService refresh(pipeline, ingestion);
        QSignalSpy spy(&refresh, &services::FredEconomicRefreshService::finished);

        providers::FredObservationRequest request;
        request.apiKey = QStringLiteral("integration-secret");
        request.seriesId = QStringLiteral("GDP");
        request.limit = 10;

        QVERIFY(refresh.refresh(request, QStringLiteral("fred.gdp"),
                                QStringLiteral("refresh-001"), 2));
        QVERIFY(refresh.isBusy());
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 5000);
        QVERIFY(!refresh.isBusy());
        QCOMPARE(server.requestCount, 1);

        const QList<QVariant> result = spy.takeFirst();
        QCOMPARE(result.at(0).toString(), QStringLiteral("refresh-001"));
        QVERIFY(result.at(1).toBool());
        QVERIFY2(result.at(2).toString().isEmpty(), qPrintable(result.at(2).toString()));
        const providers::AdapterResult adapterResult =
            qvariant_cast<providers::AdapterResult>(result.at(3));
        QVERIFY(!adapterResult.rawResult.redactedRequestArguments.join(QLatin1Char(';'))
                     .contains(QStringLiteral("integration-secret")));

        const auto persisted = store.loadDataset(QStringLiteral("fred"),
                                                 QStringLiteral("fred.gdp"), &error);
        QVERIFY2(persisted.has_value(), qPrintable(error));
        QCOMPARE(persisted->observations.size(), qsizetype(2));
        QCOMPARE(*persisted->observations.at(0).value, 100.5);
        QCOMPARE(*persisted->observations.at(1).value, 101.5);
    }
};

QTEST_GUILESS_MAIN(FredEconomicRefreshServiceTest)

#include "tst_fred_economic_refresh_service.moc"
