#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include "core/CommonTypes.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    QMainWindow mainWindow;
    mainWindow.setWindowTitle("Universal Smart Search Engine — Toolchain Verification Shell");
    mainWindow.resize(800, 500);

    auto* centralWidget = new QWidget(&mainWindow);
    auto* layout = new QVBoxLayout(centralWidget);

    auto* headerLabel = new QLabel("<h2>Universal Smart Search Engine</h2><p><b>Phase 1: Toolchain & Build Verification Shell</b></p>", centralWidget);
    headerLabel->setAlignment(Qt::AlignCenter);

    auto* statusLabel = new QLabel(
        "Build Status: Scaffold Initialized Successfully.<br>"
        "Target Platform: Windows 10/11 x64 (MSVC 2022 / C++17 / Qt 6 Widgets)<br>"
        "Core Types Loaded: sizeof(PostingNode) = " + QString::number(sizeof(sse::PostingNode)) + " bytes.<br><br>"
        "<i>Search and DSA functionality are held at gate for Phase 2.</i>",
        centralWidget
    );
    statusLabel->setAlignment(Qt::AlignCenter);

    layout->addStretch();
    layout->addWidget(headerLabel);
    layout->addWidget(statusLabel);
    layout->addStretch();

    mainWindow.setCentralWidget(centralWidget);
    mainWindow.show();

    return app.exec();
}
