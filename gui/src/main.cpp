#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    app.setApplicationName("GoofyDeck GUI");
    app.setOrganizationName("goofydeck");
    
    // Set dark theme
    app.setStyleSheet(R"(
        QMainWindow {
            background-color: #2d2d2d;
        }
        QMenuBar {
            background-color: #333;
            color: #fff;
        }
        QMenuBar::item:selected {
            background-color: #444;
        }
        QMenu {
            background-color: #333;
            color: #fff;
            border: 1px solid #444;
        }
        QMenu::item:selected {
            background-color: #444;
        }
        QTabWidget::pane {
            border: 1px solid #444;
            background-color: #2d2d2d;
        }
        QTabBar::tab {
            background-color: #333;
            color: #fff;
            padding: 8px 16px;
            border: 1px solid #444;
        }
        QTabBar::tab:selected {
            background-color: #444;
        }
        QPushButton {
            background-color: #444;
            color: #fff;
            border: 1px solid #555;
            padding: 6px 12px;
            border-radius: 4px;
        }
        QPushButton:hover {
            background-color: #555;
        }
        QPushButton:pressed {
            background-color: #333;
        }
        QComboBox {
            background-color: #333;
            color: #fff;
            border: 1px solid #555;
            padding: 4px;
        }
        QLabel {
            color: #ccc;
        }
        QStatusBar {
            background-color: #333;
            color: #fff;
        }
        QGroupBox {
            color: #fff;
            border: 1px solid #444;
            margin-top: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 8px;
            padding: 0 4px;
        }
    )");
    
    MainWindow window;
    window.show();
    
    return app.exec();
}
