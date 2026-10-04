#include <QApplication>
#include "MainWindow.h"
#include "Config.h"
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(guiLog, "goofydeck.gui")
Q_LOGGING_CATEGORY(networkLog, "goofydeck.network")
Q_LOGGING_CATEGORY(configLog, "goofydeck.config")
Q_LOGGING_CATEGORY(parserLog, "goofydeck.parser")

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    app.setApplicationName("GoofyDeck GUI");
    app.setOrganizationName("goofydeck");
    
    // Configure logging based on Config
    if (!Config::instance().enableDebugLogging()) {
        // Disable debug logging by default
        QLoggingCategory::setFilterRules("*.debug=false\n*.info=false\nqt.*.debug=false\nqt.*.info=false");
    } else {
        // Enable all logging in debug mode
        QLoggingCategory::setFilterRules("*.debug=true");
    }
    
    // Disable Qt-specific verbose logging
    QLoggingCategory::setFilterRules("qt.qpa.input.events.debug=false\nqt.pointer.velocity.debug=false\nqt.qpa.events.debug=false\nqt.widgets.painting.debug=false\nqt.widgets.focus.debug=false");
    
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
