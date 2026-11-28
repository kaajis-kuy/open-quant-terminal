#include "app/MainWindow.h"

#include <QLabel>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setObjectName("mainWindow");
    setWindowTitle("Open Quant Terminal");
    resize(1280, 800);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);

    auto* title = new QLabel("Open Quant Terminal", central);
    title->setObjectName("welcomeTitle");
    title->setAlignment(Qt::AlignCenter);

    auto* subtitle = new QLabel(
        "Quantitative research terminal — application foundation",
        central
    );
    subtitle->setObjectName("welcomeSubtitle");
    subtitle->setAlignment(Qt::AlignCenter);

    layout->addStretch();
    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addStretch();

    setCentralWidget(central);
    statusBar()->showMessage("Ready");
}
