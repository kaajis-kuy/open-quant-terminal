#include "app/MainWindow.h"

#include <QLabel>
#include <QStatusBar>
#include <QtTest>

class MainWindowTest final : public QObject {
    Q_OBJECT

private slots:
    void window_has_expected_identity() {
        MainWindow window;

        QCOMPARE(window.objectName(), QString("mainWindow"));
        QCOMPARE(window.windowTitle(), QString("Open Quant Terminal"));
    }

    void window_exposes_welcome_content() {
        MainWindow window;

        const auto* title = window.findChild<QLabel*>("welcomeTitle");
        const auto* subtitle = window.findChild<QLabel*>("welcomeSubtitle");

        QVERIFY(title != nullptr);
        QVERIFY(subtitle != nullptr);
        QVERIFY(title->text().contains("Open Quant Terminal"));
        QVERIFY(subtitle->text().contains("Quantitative research terminal"));
    }

    void window_starts_in_ready_state() {
        MainWindow window;

        QVERIFY(window.statusBar() != nullptr);
        QCOMPARE(window.statusBar()->currentMessage(), QString("Ready"));
    }
};

QTEST_MAIN(MainWindowTest)

#include "tst_main_window.moc"
