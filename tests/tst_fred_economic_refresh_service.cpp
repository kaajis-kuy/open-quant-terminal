#include "core/providers/FredPipeline.h"
#include "core/services/EconomicDataIngestionService.h"
#include "core/services/FredEconomicRefreshService.h"
#include "core/storage/SqliteCanonicalDatasetStore.h"
#include "application/DataExplorerQueries.h"

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
        return QUrl(QStringLiteral("http://127.0.0.1:%1/fred/series/observations")
                        .arg(server.serverPort()));
    }
    int requestCount = 0;
    int statusCode = 200;
    QByteArray body = R"({"count":2,"offset":0,"limit":10,"observations":[{"date":"2024-01-01","value":"100.5"},{"date":"2024-02-01","value":"101.5"}]})";
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

        // Verify the stored observations are exposed through the application
        // query/read-model boundary after the full mock-provider refresh flow.
        application::EconomicDataExplorerQuery query(
            [&store](const QString& provider, const QString& dataset,
                     qsizetype pageSize, qint64 offset, QString* queryError)
                -> std::optional<application::QueryPage<data::CanonicalDataset>> {
                const auto page = store.loadDatasetPage(
                    provider, dataset, pageSize, offset, queryError);
                if (!page) return std::nullopt;
                return application::QueryPage<data::CanonicalDataset>{
                    page->dataset, page->hasMore, page->offset, page->pageSize};
            });
        const auto readModel = query.execute(
            QStringLiteral("fred"), QStringLiteral("fred.gdp"), &error, 10, 0);
        QVERIFY2(readModel.has_value(), qPrintable(error));
        QCOMPARE(readModel->dataset.observations.size(), qsizetype(2));
        QCOMPARE(*readModel->dataset.observations.at(0).value, 100.5);
        QCOMPARE(*readModel->dataset.observations.at(1).value, 101.5);
        QVERIFY(!readModel->hasMore);
    }

    void persistsPartialDatasetAndExposesQualityMetadata() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        FredRefreshMockServer server;
        server.body = R"({"count":3,"offset":0,"limit":2,"observations":[{"date":"2024-01-01","value":"100.5"},{"date":"2024-02-01","value":"101.5"}]})";
        QVERIFY(server.listen());

        storage::SqliteCanonicalDatasetStore store(directory.filePath("economic.sqlite"));
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
        request.limit = 2;
        QVERIFY(refresh.refresh(request, "fred.gdp", "partial-refresh", 1));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 5000);
        const QList<QVariant> args = spy.takeFirst();
        QVERIFY2(args.at(1).toBool(), qPrintable(args.at(2).toString()));
        const auto adapterResult = qvariant_cast<providers::AdapterResult>(args.at(3));
        QVERIFY(adapterResult.dataset.has_value());
        QCOMPARE(adapterResult.dataset->quality.status, data::DataStatus::Partial);
        QVERIFY(adapterResult.dataset->quality.isPartial);
        QVERIFY(adapterResult.dataset->quality.isTruncated);
        QVERIFY(!adapterResult.dataset->quality.nextPageCursor.isEmpty());

        const auto persisted = store.loadDataset("fred", "fred.gdp", &error);
        QVERIFY2(persisted.has_value(), qPrintable(error));
        QCOMPARE(persisted->observations.size(), qsizetype(2));
        QCOMPARE(persisted->quality.status, data::DataStatus::Partial);
        QVERIFY(persisted->quality.isTruncated);

        application::EconomicDataExplorerQuery query(
            [&store](const QString& provider, const QString& dataset,
                     qsizetype pageSize, qint64 offset, QString* queryError)
                -> std::optional<application::QueryPage<data::CanonicalDataset>> {
                const auto page = store.loadDatasetPage(provider, dataset, pageSize, offset, queryError);
                if (!page) return std::nullopt;
                return application::QueryPage<data::CanonicalDataset>{
                    page->dataset, page->hasMore, page->offset, page->pageSize};
            });
        const auto readModel = query.execute("fred", "fred.gdp", &error, 10, 0);
        QVERIFY2(readModel.has_value(), qPrintable(error));
        QCOMPARE(readModel->dataset.quality.status, data::DataStatus::Partial);
        QVERIFY(readModel->dataset.quality.isTruncated);
    }

    void reportsProviderFailureWithoutPersisting() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        FredRefreshMockServer server;
        server.statusCode = 403;
        QVERIFY(server.listen());

        storage::SqliteCanonicalDatasetStore store(directory.filePath("economic.sqlite"));
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
        QVERIFY(refresh.refresh(request, "fred.gdp", "failed-refresh", 1));
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 5000);
        const QList<QVariant> args = spy.takeFirst();
        QVERIFY(!args.at(1).toBool());
        QCOMPARE(qvariant_cast<providers::AdapterResult>(args.at(3)).responseStatus,
                 providers::ResponseStatus::Failed);
        QVERIFY(!store.loadDataset("fred", "fred.gdp", &error).has_value());
    }

    void cancellationCompletesWithoutPersisting() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        FredRefreshMockServer server;
        QVERIFY(server.listen());

        storage::SqliteCanonicalDatasetStore store(directory.filePath("economic.sqlite"));
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
        QVERIFY(refresh.refresh(request, "fred.gdp", "cancelled-refresh", 1));
        QVERIFY(refresh.isBusy());
        refresh.cancel();
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 1, 5000);
        QVERIFY(!refresh.isBusy());
        const QList<QVariant> args = spy.takeFirst();
        QVERIFY(!args.at(1).toBool());
        QCOMPARE(qvariant_cast<providers::AdapterResult>(args.at(3)).responseStatus,
                 providers::ResponseStatus::Failed);
        QVERIFY(!store.loadDataset("fred", "fred.gdp", &error).has_value());
    }
};

QTEST_GUILESS_MAIN(FredEconomicRefreshServiceTest)

#include "tst_fred_economic_refresh_service.moc"
