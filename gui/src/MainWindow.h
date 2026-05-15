#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include "GoofyDeckClient.h"
#include "YamlParser.h"
#include "ButtonEditDialog.h"
#include "PageCreateDialog.h"
#include "PageEditDialog.h"
#include "PresetEditDialog.h"
#include "ConfigEditorDialog.h"

class DeckWidget;
class GoofyDeckClient;
class QPlainTextEdit;
class QTextEdit;
class QLabel;
class QPushButton;
class QComboBox;
class QLineEdit;
class QTabWidget;
class YamlHighlighter;

struct ButtonConfig {
    QString name;
    QString icon;
    QString action;
    QString entityId;
    QStringList presets;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
    // Called by DeckWidget when button is right-clicked
    void showButtonContextMenu(int buttonIndex, const QPoint &globalPos);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onTestConnection();
    void onLogMessage(const QString &msg);
    void onLoadConfig();
    void onSaveConfig();
    void onReloadPages();
    void onStopControl();
    void onStartControl();
    void onPageChanged(int index);
    void onDeckPageChanged(const QString &pageName);
    
    // Context menu actions
    void onEditButton(int buttonIndex);
    void onDeleteButton(int buttonIndex);
    void onEditPage(const QString &pageName);
    void onDeletePage(const QString &pageName);
    void onCreatePage();
    void onEditPresets();
    void onGlobalEditor();

private:
    void setupUI();
    void setupMenuBar();
    void setupCentralWidget();
    void setupConnections();
    void loadConfiguration();
    void updateButtonLabels();
    
    GoofyDeckClient *m_client = nullptr;
    DeckWidget *m_deckWidget = nullptr;
    
    // UI Elements
    QTabWidget *m_tabWidget = nullptr;
    QPlainTextEdit *m_logView = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_connectBtn = nullptr;
    QPushButton *m_disconnectBtn = nullptr;
    
    // Data
    QString m_configPath;
    QMap<int, ButtonConfig> m_buttonConfigs;
    QStringList m_pages;
    YamlParser m_yamlParser;
    QString m_currentPage;
};

#endif // MAINWINDOW_H
