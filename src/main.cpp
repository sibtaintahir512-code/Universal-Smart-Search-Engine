#include <QApplication>
#include "gui/MainWindow.hpp"
#include "gui/ThemeManager.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    QApplication::setOrganizationName("UniversalSearchEngine");
    QApplication::setApplicationName("SearchEngineApp");
    QApplication::setApplicationVersion("1.0.0");

    // Apply default dark theme before rendering
    sse::gui::ThemeManager::applyTheme(sse::gui::ThemeMode::Dark);

    sse::gui::MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
