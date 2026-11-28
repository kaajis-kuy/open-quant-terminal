#include "app/AppCompositionRoot.h"
#include "app/MainWindow.h"

#include <QApplication>
#include <QStatusBar>


int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Open Quant Terminal");
    QApplication::setOrganizationName("OpenQuantTerminal");

    // Keep the composition root alive for the entire event-loop lifetime.
    oqt::app::AppCompositionRoot composition;
    (void)composition.initialize();

    MainWindow window(composition.marketExplorerQuery(),
                       composition.economicDataExplorerQuery(),
                       composition.marketInstrumentCatalogQuery(),
                       composition.economicDatasetCatalogQuery(),
                       &composition);
    const QString marketState = composition.marketStatus().available
        ? QStringLiteral("Market data: available")
        : QStringLiteral("Market data: disabled");
    const QString economicState = composition.economicStatus().available
        ? QStringLiteral("Economic data: available")
        : QStringLiteral("Economic data: disabled (%1)")
              .arg(composition.economicStatus().errorMessage);
    const QString marketMessage = composition.marketStatus().available
        ? marketState
        : QStringLiteral("Market data: disabled (%1)")
              .arg(composition.marketStatus().errorMessage);
    window.statusBar()->showMessage(marketMessage + QStringLiteral(" | ") + economicState);
    window.show();

    return app.exec();
}
