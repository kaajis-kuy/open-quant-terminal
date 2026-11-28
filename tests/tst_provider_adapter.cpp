#include "core/providers/ProviderAdapter.h"

#include <QtTest>

using namespace oqt::providers;

namespace {

class FakeFredAdapter final : public ProviderAdapter {
public:
    QString providerId() const override { return QStringLiteral("fred"); }
    QStringList supportedOperations() const override {
        return {QStringLiteral("series"), QStringLiteral("observations")};
    }

    AdapterResult normalize(const ProviderResponse& response,
                            const ProviderRequestContext& context) const override {
        Q_UNUSED(context);
        AdapterResult result;
        result.responseStatus = response.status;
        return result;
    }
};

class InvalidIdAdapter final : public ProviderAdapter {
public:
    QString providerId() const override { return QStringLiteral("  "); }
    QStringList supportedOperations() const override { return {}; }
    AdapterResult normalize(const ProviderResponse&, const ProviderRequestContext&) const override {
        return {};
    }
};

} // namespace

class ProviderAdapterTest final : public QObject {
    Q_OBJECT

private slots:
    void supportsDeclaredOperations() {
        FakeFredAdapter adapter;
        QVERIFY(adapter.supportsOperation(QStringLiteral("series")));
        QVERIFY(adapter.supportsOperation(QStringLiteral("observations")));
        QVERIFY(!adapter.supportsOperation(QStringLiteral("search")));
        QVERIFY(!adapter.supportsOperation(QString()));
    }

    void registryRegistersAndFindsAdapter() {
        ProviderAdapterRegistry registry;
        auto adapter = std::make_shared<FakeFredAdapter>();
        QVERIFY(registry.registerAdapter(adapter));
        QCOMPARE(registry.size(), qsizetype(1));
        QCOMPARE(registry.adapter(QStringLiteral("fred"))->providerId(), QStringLiteral("fred"));
        QCOMPARE(registry.providerIds(), QStringList{QStringLiteral("fred")});
    }

    void registryRejectsDuplicateProvider() {
        ProviderAdapterRegistry registry;
        QVERIFY(registry.registerAdapter(std::make_shared<FakeFredAdapter>()));
        QVERIFY(!registry.registerAdapter(std::make_shared<FakeFredAdapter>()));
        QCOMPARE(registry.size(), qsizetype(1));
    }

    void registryRejectsNullAndBlankIds() {
        ProviderAdapterRegistry registry;
        QVERIFY(!registry.registerAdapter(nullptr));
        QVERIFY(!registry.registerAdapter(std::make_shared<InvalidIdAdapter>()));
        QCOMPARE(registry.size(), qsizetype(0));
    }

    void missingAdapterReturnsEmptyPointer() {
        ProviderAdapterRegistry registry;
        QVERIFY(!registry.adapter(QStringLiteral("unknown")));
    }
};

QTEST_GUILESS_MAIN(ProviderAdapterTest)

#include "tst_provider_adapter.moc"
