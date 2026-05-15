#include "MainWindow.h"
#include "DeckWidget.h"
#include "GoofyDeckClient.h"
#include "ButtonEditDialog.h"
#include "PageCreateDialog.h"
#include "YamlHighlighter.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QCloseEvent>
#include <QSplitter>
#include <QGroupBox>
#include <QScrollArea>
#include <QDateTime>
#include <QDebug>
#include <QMenu>
#include <QFormLayout>
#include <QAction>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_client(new GoofyDeckClient(this))
    , m_configPath("../config/configuration.yml")
{
    setupUI();
    setupConnections();
    loadConfiguration();
    
    setWindowTitle("GoofyDeck GUI");
    resize(900, 700);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI()
{
    // Menu bar
    QMenuBar *menuBar = new QMenuBar(this);
    setMenuBar(menuBar);
    
    QMenu *fileMenu = menuBar->addMenu("&File");
    fileMenu->addAction("&Load Config", this, &MainWindow::onLoadConfig, QKeySequence::Open);
    fileMenu->addAction("&Save Config", this, &MainWindow::onSaveConfig, QKeySequence::Save);
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, &QWidget::close, QKeySequence::Quit);
    
    QMenu *controlMenu = menuBar->addMenu("&Control");
    controlMenu->addAction("&Test Connection", this, &MainWindow::onTestConnection);
    controlMenu->addSeparator();
    controlMenu->addAction("Stop &Control", this, &MainWindow::onStopControl);
    controlMenu->addAction("Start C&ontrol", this, &MainWindow::onStartControl);
    controlMenu->addSeparator();
    controlMenu->addAction("&Reload Pages", this, &MainWindow::onReloadPages);
    
    QMenu *configMenu = menuBar->addMenu("&Config");
    configMenu->addAction("&Edit Presets...", this, &MainWindow::onEditPresets);
    configMenu->addAction("&Global Editor...", this, &MainWindow::onGlobalEditor);
    
    // Central widget with tabs
    m_tabWidget = new QTabWidget(this);
    setCentralWidget(m_tabWidget);
    
    // Tab 1: Deck Control
    QWidget *deckTab = new QWidget();
    QVBoxLayout *deckLayout = new QVBoxLayout(deckTab);
    
    // Toolbar
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    
    QPushButton *testBtn = new QPushButton("Test Daemon", this);
    testBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; padding: 8px 16px; }");
    connect(testBtn, &QPushButton::clicked, this, &MainWindow::onTestConnection);
    toolbarLayout->addWidget(testBtn);
    
    QPushButton *refreshBtn = new QPushButton("Refresh Icons", this);
    refreshBtn->setStyleSheet("QPushButton { background-color: #2196F3; color: white; padding: 8px 16px; }");
    connect(refreshBtn, &QPushButton::clicked, this, [this]() { m_deckWidget->refreshIcons(); });
    toolbarLayout->addWidget(refreshBtn);
    
    toolbarLayout->addStretch();
    
    deckLayout->addLayout(toolbarLayout);
    
    // Deck widget
    m_deckWidget = new DeckWidget(m_client, this);
    deckLayout->addWidget(m_deckWidget, 0, Qt::AlignCenter);
    
    // Status
    QHBoxLayout *statusLayout = new QHBoxLayout();
    m_statusLabel = new QLabel("Ready");
    m_statusLabel->setStyleSheet("QLabel { color: #4CAF50; font-weight: bold; }");
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    deckLayout->addLayout(statusLayout);
    
    m_tabWidget->addTab(deckTab, "Deck Control");
    
    // Tab 2: Event Log
    QWidget *logTab = new QWidget();
    QVBoxLayout *logLayout = new QVBoxLayout(logTab);
    
    QHBoxLayout *logToolbar = new QHBoxLayout();
    QPushButton *clearLogBtn = new QPushButton("Clear", this);
    logToolbar->addWidget(clearLogBtn);
    logToolbar->addStretch();
    logLayout->addLayout(logToolbar);
    
    m_logView = new QPlainTextEdit(this);
    m_logView->setReadOnly(true);
    m_logView->setFont(QFont("Consolas", 9));
    m_logView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_logView->setStyleSheet(R"(
        QPlainTextEdit {
            background-color: #1a1a1a;
            color: #00ff00;
            border: 1px solid #444;
        }
    )");
    logLayout->addWidget(m_logView);
    
    connect(clearLogBtn, &QPushButton::clicked, m_logView, &QPlainTextEdit::clear);
    
    m_tabWidget->addTab(logTab, "Event Log");
    
    // Status bar
    statusBar()->showMessage("Ready");
    
    // Set fixed size to fit content tightly
    // Deck: 556px wide, 390px tall + toolbar/status/tab = ~470px total
    resize(580, 520);
    setFixedSize(580, 520);
}

void MainWindow::setupConnections()
{
    connect(m_client, &GoofyDeckClient::errorOccurred, this, &MainWindow::onLogMessage);
    connect(m_deckWidget, &DeckWidget::pageChanged, this, &MainWindow::onDeckPageChanged);
    connect(m_deckWidget, &DeckWidget::buttonRightClicked,
            this, &MainWindow::showButtonContextMenu);
}

void MainWindow::onTestConnection()
{
    if (m_client->ping()) {
        m_statusLabel->setText("Daemon OK");
        m_statusLabel->setStyleSheet("QLabel { color: #4CAF50; font-weight: bold; }");
        onLogMessage("Daemon ping: OK");
    } else {
        m_statusLabel->setText("Daemon unavailable");
        m_statusLabel->setStyleSheet("QLabel { color: #f44336; font-weight: bold; }");
        onLogMessage(QString("Daemon ping failed: %1").arg(m_client->lastError()));
    }
}

void MainWindow::onLogMessage(const QString &msg)
{
    m_logView->appendPlainText(msg);
}

void MainWindow::onLoadConfig()
{
    QString path = QFileDialog::getOpenFileName(this, "Open Configuration", 
        m_configPath, "YAML Files (*.yml *.yaml);;All Files (*)");
    
    if (path.isEmpty()) return;
    
    m_configPath = path;
    loadConfiguration();
}

void MainWindow::onSaveConfig()
{
    // No longer needed - config editor is in separate dialog
}

void MainWindow::onReloadPages()
{
    m_client->loadLastPage();
    statusBar()->showMessage("Pages reloaded", 3000);
}

void MainWindow::onStopControl()
{
    if (m_client->stopControl()) {
        statusBar()->showMessage("Control stopped - miniapp mode active", 3000);
    }
}

void MainWindow::onStartControl()
{
    if (m_client->startControl()) {
        statusBar()->showMessage("Control started", 3000);
    }
}

void MainWindow::showButtonContextMenu(int buttonIndex, const QPoint &globalPos)
{
    QMenu menu(this);
    
    // Simulation actions
    QAction *tapAction = menu.addAction("Simulate TAP");
    connect(tapAction, &QAction::triggered, this, [this, buttonIndex]() {
        m_client->simulateButton(buttonIndex, ButtonEvent::TAP);
        onLogMessage(QString("Button %1 TAP simulated").arg(buttonIndex));
    });
    
    QAction *holdAction = menu.addAction("Simulate HOLD");
    connect(holdAction, &QAction::triggered, this, [this, buttonIndex]() {
        m_client->simulateButton(buttonIndex, ButtonEvent::HOLD);
        onLogMessage(QString("Button %1 HOLD simulated").arg(buttonIndex));
    });
    
    QAction *longHoldAction = menu.addAction("Simulate LONGHOLD");
    connect(longHoldAction, &QAction::triggered, this, [this, buttonIndex]() {
        m_client->simulateButton(buttonIndex, ButtonEvent::LONGHOLD);
        onLogMessage(QString("Button %1 LONGHOLD simulated").arg(buttonIndex));
    });
    
    QAction *releaseAction = menu.addAction("Simulate RELEASED");
    connect(releaseAction, &QAction::triggered, this, [this, buttonIndex]() {
        m_client->simulateButton(buttonIndex, ButtonEvent::RELEASED);
        onLogMessage(QString("Button %1 RELEASED simulated").arg(buttonIndex));
    });
    
    menu.addSeparator();
    
    QAction *editAction = menu.addAction("Edit Button");
    connect(editAction, &QAction::triggered, this, [this, buttonIndex]() {
        onEditButton(buttonIndex);
    });
    
    QAction *deleteAction = menu.addAction("Delete Button");
    connect(deleteAction, &QAction::triggered, this, [this, buttonIndex]() {
        onDeleteButton(buttonIndex);
    });
    
    // Check if button navigates to a configured page
    QString targetPage;
    if (m_buttonConfigs.contains(buttonIndex)) {
        const ButtonConfig &config = m_buttonConfigs[buttonIndex];
        // Check if action is $page.go_to and entityId is a valid page
        if (config.action == "$page.go_to" && !config.entityId.isEmpty()) {
            QStringList pages = m_yamlParser.pageNames();
            if (pages.contains(config.entityId)) {
                targetPage = config.entityId;
            }
        }
    }
    
    if (!targetPage.isEmpty()) {
        menu.addSeparator();
        QAction *editPageAction = menu.addAction(QString("Edit Page '%1'").arg(targetPage));
        connect(editPageAction, &QAction::triggered, this, [this, targetPage]() {
            onEditPage(targetPage);
        });
        
        QAction *deletePageAction = menu.addAction(QString("Delete Page '%1'").arg(targetPage));
        connect(deletePageAction, &QAction::triggered, this, [this, targetPage]() {
            onDeletePage(targetPage);
        });
    }
    
    menu.addSeparator();
    
    QAction *createPageAction = menu.addAction("Create Page");
    connect(createPageAction, &QAction::triggered, this, &MainWindow::onCreatePage);
    
    menu.exec(globalPos);
}

void MainWindow::onEditButton(int buttonIndex)
{
    ButtonConfig config;
    qDebug() << "[onEditButton] buttonIndex:" << buttonIndex << "m_buttonConfigs size:" << m_buttonConfigs.size();
    if (m_buttonConfigs.contains(buttonIndex)) {
        config = m_buttonConfigs[buttonIndex];
        qDebug() << "[onEditButton] Found config - name:" << config.name << "action:" << config.action << "entityId:" << config.entityId;
    } else {
        qDebug() << "[onEditButton] No config found for button" << buttonIndex;
    }
    
    ButtonEditDialog dialog(this);
    
    // Get available presets from parser and pass to dialog
    QStringList availablePresets = m_yamlParser.presetNames();
    dialog.setAvailablePresets(availablePresets);
    
    dialog.setButtonConfig(buttonIndex, config);
    
    if (dialog.exec() == QDialog::Accepted) {
        ButtonConfig newConfig = dialog.getButtonConfig();
        newConfig.presets = dialog.getSelectedPresets();
        m_buttonConfigs[buttonIndex] = newConfig;
        
        // Update deck widget
        m_deckWidget->updateButtonConfig(buttonIndex, newConfig.name, newConfig.icon);
        
        // Save to YAML
        YamlButton yamlBtn;
        yamlBtn.name = newConfig.name;
        yamlBtn.icon = newConfig.icon;
        yamlBtn.tapAction = newConfig.action;
        yamlBtn.entityId = newConfig.entityId;
        yamlBtn.presets = newConfig.presets;
        
        QString content;
        if (!m_configPath.isEmpty()) {
            QFile file(m_configPath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                content = QString::fromUtf8(file.readAll());
                file.close();
            }
        }
        QString newContent = m_yamlParser.updateButtonInYaml(content, m_currentPage, buttonIndex, yamlBtn);
        
        if (newContent != content) {
            if (m_yamlParser.saveToFile(m_configPath, newContent)) {
                onLogMessage(QString("Button %1 saved to YAML").arg(buttonIndex));
                statusBar()->showMessage(QString("Button %1 saved to %2").arg(buttonIndex).arg(m_configPath), 3000);
            } else {
                QMessageBox::critical(this, "Error", "Failed to save configuration file");
            }
        }
        
        onLogMessage(QString("Button %1 updated").arg(buttonIndex));
    }
}

void MainWindow::onDeleteButton(int buttonIndex)
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Delete Button",
        QString("Are you sure you want to delete button %1?").arg(buttonIndex),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        // Remove from button configs
        m_buttonConfigs.remove(buttonIndex);
        
        // Clear button on deck
        m_deckWidget->clearButtonConfig(buttonIndex);
        
        // Remove from YAML
        QString content;
        if (!m_configPath.isEmpty()) {
            QFile file(m_configPath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                content = QString::fromUtf8(file.readAll());
                file.close();
            }
        }
        QString newContent = m_yamlParser.deleteButtonFromYaml(content, m_currentPage, buttonIndex);
        
        if (newContent != content) {
            if (m_yamlParser.saveToFile(m_configPath, newContent)) {
                onLogMessage(QString("Button %1 deleted from YAML").arg(buttonIndex));
                statusBar()->showMessage(QString("Button %1 deleted from %2").arg(buttonIndex).arg(m_configPath), 3000);
            } else {
                QMessageBox::critical(this, "Error", "Failed to save configuration file");
            }
        }
    }
}

void MainWindow::onDeletePage(const QString &pageName)
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Delete Page",
        QString("Are you sure you want to delete page '%1'?\n\nThis action cannot be undone.").arg(pageName),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        // Remove page from YAML
        QString content;
        if (!m_configPath.isEmpty()) {
            QFile file(m_configPath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                content = QString::fromUtf8(file.readAll());
                file.close();
            }
        }
        QString newContent = m_yamlParser.deletePageFromYaml(content, pageName);
        
        if (newContent != content) {
            if (m_yamlParser.saveToFile(m_configPath, newContent)) {
                onLogMessage(QString("Page '%1' deleted from YAML").arg(pageName));
                statusBar()->showMessage(QString("Page '%1' deleted from %2").arg(pageName).arg(m_configPath), 3000);
                
                // Reload configuration to update internal state
                loadConfiguration();
            } else {
                QMessageBox::critical(this, "Error", "Failed to save configuration file");
            }
        }
    }
}

void MainWindow::onCreatePage()
{
    PageCreateDialog dialog(this);
    
    if (dialog.exec() == QDialog::Accepted) {
        QString pageName = dialog.getPageName();
        QString wallpaperPath = dialog.getWallpaperPath();
        QString layoutType = dialog.getLayoutType();
        
        // Add page to YAML configuration
        QString content;
        if (!m_configPath.isEmpty()) {
            QFile file(m_configPath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                content = QString::fromUtf8(file.readAll());
                file.close();
            }
        }
        QString newContent = m_yamlParser.addPageToYaml(content, pageName, wallpaperPath, layoutType);
        
        if (newContent != content) {
            if (m_yamlParser.saveToFile(m_configPath, newContent)) {
                onLogMessage(QString("Created new page: %1").arg(pageName));
                if (!wallpaperPath.isEmpty()) {
                    onLogMessage(QString("  Wallpaper: %1").arg(wallpaperPath));
                }
                onLogMessage(QString("  Layout: %1").arg(layoutType));
                statusBar()->showMessage(QString("Page '%1' created and saved to YAML").arg(pageName), 3000);
                
                // Reload configuration to update internal state
                loadConfiguration();
            } else {
                QMessageBox::critical(this, "Error", "Failed to save configuration file");
            }
        } else {
            statusBar()->showMessage(QString("Page '%1' already exists or could not be created").arg(pageName), 3000);
        }
    }
}

void MainWindow::onEditPage(const QString &pageName)
{
    // Get page data from parser
    YamlPage page = m_yamlParser.page(pageName);
    
    // Extract wallpaper path from page data (simplified - would need proper parsing)
    QString wallpaperPath;
    QString layoutType = "3x5"; // default
    
    PageEditDialog dialog(this);
    dialog.setPageConfig(pageName, wallpaperPath, layoutType);
    
    if (dialog.exec() == QDialog::Accepted) {
        QString newWallpaper = dialog.getWallpaperPath();
        QString newLayout = dialog.getLayoutType();
        
        // TODO: Update page in YAML configuration
        onLogMessage(QString("Updated page '%1'").arg(pageName));
        if (!newWallpaper.isEmpty()) {
            onLogMessage(QString("  New wallpaper: %1").arg(newWallpaper));
        }
        onLogMessage(QString("  New layout: %1").arg(newLayout));
        
        statusBar()->showMessage(QString("Page '%1' updated (not yet saved to YAML)").arg(pageName), 3000);
    }
}

void MainWindow::onPageChanged(int index)
{
    if (index < 0) return;
    
    // Get all pages and find the one at this index
    QStringList pages = m_yamlParser.pageNames();
    if (index >= pages.size()) return;
    
    QString pageName = pages[index];
    m_currentPage = pageName;
    
    onLogMessage(QString("Selected page: %1").arg(pageName));
    
    // Get page data from parser
    YamlPage page = m_yamlParser.page(pageName);
    qDebug() << "[onPageChanged] page:" << pageName << "buttons count:" << page.buttons.size();
    
    // Clear current button configs
    m_buttonConfigs.clear();
    
    // Update button configs from page data
    for (int i = 0; i < page.buttons.size() && i < 14; ++i) {
        const YamlButton &btn = page.buttons[i];
        qDebug() << "[onPageChanged] Button" << (i+1) << "name:" << btn.name << "action:" << btn.tapAction << "entityId:" << btn.entityId;
        ButtonConfig config;
        config.name = btn.name;
        config.icon = btn.icon;
        config.action = btn.tapAction;
        config.entityId = btn.entityId;
        config.presets = btn.presets;
        m_buttonConfigs[i + 1] = config;
        
        // Update deck widget button
        m_deckWidget->updateButtonConfig(i + 1, btn.name, btn.icon);
        
        // Register button for state tracking (HA entities, sensors)
        if (!btn.entityId.isEmpty()) {
            QStringList presets;
            for (const auto &state : btn.states.keys()) {
                presets << btn.states[state].presets;
            }
            m_deckWidget->registerButtonForStateTracking(i + 1, pageName, btn.entityId, presets);
        }
    }
    
    qDebug() << "[onPageChanged] Loaded" << m_buttonConfigs.size() << "button configs";
    
    // Update deck widget current page
    m_deckWidget->setCurrentPage(pageName);
    
    statusBar()->showMessage(QString("Page '%1' loaded with %2 buttons").arg(pageName).arg(page.buttons.size()), 3000);
}

void MainWindow::onDeckPageChanged(const QString &pageName)
{
    qDebug() << "[onDeckPageChanged] Page changed to:" << pageName;
    
    // Update current page
    m_currentPage = pageName;
    
    // Get page data from parser
    YamlPage page = m_yamlParser.page(pageName);
    qDebug() << "[onDeckPageChanged] page:" << pageName << "buttons count:" << page.buttons.size();
    
    // Clear current button configs
    m_buttonConfigs.clear();
    
    // Update button configs from page data AND update UI
    for (int i = 0; i < page.buttons.size() && i < 14; ++i) {
        const YamlButton &btn = page.buttons[i];
        ButtonConfig config;
        config.name = btn.name;
        config.icon = btn.icon;
        config.action = btn.tapAction;
        config.entityId = btn.entityId;
        config.presets = btn.presets;
        m_buttonConfigs[i + 1] = config;
        
        // Update deck widget button name
        m_deckWidget->updateButtonConfig(i + 1, btn.name, btn.icon);
        
        // Register button for state tracking (HA entities, sensors)
        if (!btn.entityId.isEmpty()) {
            QStringList presets;
            for (const auto &state : btn.states.keys()) {
                presets << btn.states[state].presets;
            }
            m_deckWidget->registerButtonForStateTracking(i + 1, pageName, btn.entityId, presets);
        }
    }
    
    // Clear labels for buttons not present in new page
    for (int i = page.buttons.size() + 1; i <= 14; ++i) {
        m_deckWidget->clearButtonConfig(i);
    }
    
    // Refresh icons AFTER registering all buttons so StateManager info is available
    m_deckWidget->refreshIcons();
    
    qDebug() << "[onDeckPageChanged] Loaded" << m_buttonConfigs.size() << "button configs for" << pageName;
}

void MainWindow::loadConfiguration()
{
    // Try multiple possible config locations
    QStringList possiblePaths;
    
    // If m_configPath is set and exists, use it
    if (!m_configPath.isEmpty() && QFile::exists(m_configPath)) {
        possiblePaths << m_configPath;
    }
    
    // Add standard locations
    possiblePaths << "../config/configuration.yml";
    possiblePaths << "../../config/configuration.yml";
    possiblePaths << "./config/configuration.yml";
    possiblePaths << QDir::home().filePath("GoofyDeck/config/configuration.yml");
    
    QString foundPath;
    for (const QString &path : possiblePaths) {
        if (QFile::exists(path)) {
            foundPath = path;
            break;
        }
    }
    
    if (foundPath.isEmpty()) {
        onLogMessage("Configuration file not found. Searched in:");
        for (const QString &path : possiblePaths) {
            onLogMessage(QString("  - %1").arg(QDir(path).absolutePath()));
        }
        return;
    }
    
    m_configPath = foundPath;
    
    QFile file(m_configPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        onLogMessage(QString("Failed to load config: %1").arg(file.errorString()));
        return;
    }
    
    QTextStream stream(&file);
    QString content = stream.readAll();
    file.close();
    
    onLogMessage(QString("Configuration loaded from: %1").arg(m_configPath));
    
    // Parse YAML configuration
    if (m_yamlParser.parseString(content)) {
        QStringList pages = m_yamlParser.pageNames();
        onLogMessage(QString("Loaded %1 pages").arg(pages.size()));
        qDebug() << "[loadConfiguration] Pages found:" << pages;
        
        // Get actual current page from daemon session cache
        QString currentPage = m_deckWidget->getCurrentPageFromCache();
        qDebug() << "[loadConfiguration] Current page from cache:" << currentPage;
        
        if (!currentPage.isEmpty() && pages.contains(currentPage)) {
            // Load the actual current page
            int pageIndex = pages.indexOf(currentPage);
            qDebug() << "[loadConfiguration] Loading page at index:" << pageIndex;
            onPageChanged(pageIndex);
            onLogMessage(QString("Loaded current page: %1").arg(currentPage));
        } else if (!pages.isEmpty()) {
            // Fallback to first page if current page not found
            qDebug() << "[loadConfiguration] Current page not found, falling back to first page";
            onPageChanged(0);
        }
    } else {
        onLogMessage("Failed to parse YAML configuration");
    }
}

void MainWindow::updateButtonLabels()
{
    // TODO: Update button labels from parsed config
    for (int i = 1; i <= 14; ++i) {
        m_deckWidget->updateButtonConfig(i, QString("Btn %1").arg(i), "");
    }
}

void MainWindow::onEditPresets()
{
    PresetEditDialog dialog(this);
    
    // Load current YAML content from file
    QString content;
    if (!m_configPath.isEmpty()) {
        QFile file(m_configPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            content = QString::fromUtf8(file.readAll());
            file.close();
        }
    }
    dialog.loadFromYaml(content);
    
    if (dialog.exec() == QDialog::Accepted) {
        // Get modified YAML content
        QString newContent = dialog.getModifiedYaml();
        
        if (newContent != content) {
            // Save to file
            if (m_yamlParser.saveToFile(m_configPath, newContent)) {
                onLogMessage("Presets saved to YAML configuration");
                statusBar()->showMessage("Presets saved successfully", 3000);
                
                // Reload configuration to update internal state
                loadConfiguration();
            } else {
                QMessageBox::critical(this, "Error", "Failed to save presets to configuration file");
            }
        }
    }
}

void MainWindow::onGlobalEditor()
{
    ConfigEditorDialog dialog(this);
    dialog.setConfigPath(m_configPath);
    
    // Load current config
    if (!m_configPath.isEmpty()) {
        QFile file(m_configPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QString::fromUtf8(file.readAll());
            file.close();
            dialog.setConfigContent(content);
        }
    }
    
    // Connect signal to reload config when saved
    connect(&dialog, &ConfigEditorDialog::configSaved, this, [this](const QString &) {
        loadConfiguration();
    });
    
    dialog.exec();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    event->accept();
}
