#include "DeckWidget.h"
#include "ButtonWidget.h"
#include "GoofyDeckClient.h"
#include "IconCache.h"
#include "StateManager.h"
#include "HaClient.h"
#include <QGridLayout>
#include <QDebug>
#include <QApplication>
#include <QTimer>

DeckWidget::DeckWidget(GoofyDeckClient *client, QWidget *parent)
    : QWidget(parent)
    , m_client(client)
    , m_iconCache(new IconCache(this))
    , m_stateManager(new StateManager(this))
    , m_haClient(new HaClient(this))
{
    setupUI();
    
    // Setup state tracking with HaClient
    m_stateManager->setHaClient(m_haClient);
    
    // Load HA config from .env file
    m_haClient->loadConfig();
    
    // Connect state manager signals
    connect(m_stateManager, &StateManager::buttonStateChanged,
            this, &DeckWidget::onButtonStateChanged);
    connect(m_stateManager, &StateManager::buttonTextChanged,
            this, &DeckWidget::onButtonTextChanged);
    connect(m_stateManager, &StateManager::buttonNeedsRefresh,
            this, &DeckWidget::onButtonNeedsRefresh);
    
    // Try to connect to HA daemon
    if (m_haClient->isConfigured()) {
        m_haClient->connectToDaemon();
    }
    
    // Try to find session cache
    if (m_iconCache->findSessionCache()) {
        qDebug() << "Session cache found, loading icons...";
        
        // Connect page change signal
        connect(m_iconCache, &IconCache::currentPageChanged, 
                this, &DeckWidget::onPageChanged);
        
        // Connect icon update signal (for dynamic text/state updates)
        connect(m_iconCache, &IconCache::iconUpdated,
                this, &DeckWidget::onIconUpdated);
        
        // Start watching for page changes
        m_iconCache->startPageWatcher();
        
        // Load current page and icons
        m_currentPage = m_iconCache->getCurrentPage();
        qDebug() << "Current page:" << m_currentPage;
        loadIconsFromCache();
        
        // Start state tracking after initial load
        QTimer::singleShot(100, this, &DeckWidget::setupStateTracking);
    }
}

void DeckWidget::setupUI()
{
    setStyleSheet("background-color: #1a1a1a;");
    
    QGridLayout *grid = new QGridLayout(this);
    grid->setSpacing(8);
    grid->setContentsMargins(12, 12, 12, 12);
    
    // Create 14 buttons in Ulanzi D200 layout
    // Layout: 5x2 grid for buttons 1-10, then 3+1 for buttons 11-14
    // Button 14 is rectangular (442x196, ratio ~2.25:1)
    for (int i = 0; i < BUTTON_COUNT; ++i) {
        ButtonWidget *btn = new ButtonWidget(i + 1, this);
        
        connect(btn, &ButtonWidget::tapped, this, &DeckWidget::onButtonTapped);
        connect(btn, &ButtonWidget::held, this, &DeckWidget::onButtonHeld);
        connect(btn, &ButtonWidget::longHeld, this, &DeckWidget::onButtonLongHeld);
        connect(btn, &ButtonWidget::released, this, &DeckWidget::onButtonReleased);
        connect(btn, &ButtonWidget::rightClicked, this, [this, i](const QPoint &pos) {
            emit buttonRightClicked(i + 1, pos);
        });
        
        m_buttons.append(btn);
        
        // Ulanzi D200 layout:
        // Row 0: buttons 1-5 (indices 0-4)
        // Row 1: buttons 6-10 (indices 5-9)
        // Row 2: buttons 11-13 at cols 0-2, button 14 at cols 3-4 (spans 2 columns)
        if (i < 10) {
            int row = i / 5;
            int col = i % 5;
            grid->addWidget(btn, row, col);
        } else if (i < 13) {
            // Buttons 11, 12, 13 (indices 10, 11, 12) at row 2, cols 0-2
            int col = i - 10;
            grid->addWidget(btn, 2, col);
        } else {
            // Button 14 (index 13) at row 2, col 3, spanning 2 columns
            // This makes it rectangular as on the real device (442x196 ratio)
            grid->addWidget(btn, 2, 3, 1, 2);
        }
    }
    
    // Set column stretch to ensure button 14 has correct proportions
    // Button 14 spans 2 columns, so it should be ~2x wider than others
    for (int col = 0; col < 5; ++col) {
        grid->setColumnStretch(col, 1);
    }
}

void DeckWidget::loadIconsFromCache()
{
    if (!m_iconCache || !m_iconCache->hasSessionCache()) {
        qDebug() << "No icon cache available";
        return;
    }
    
    qDebug() << "Loading icons from cache for page:" << m_currentPage;
    
    // Step 1: First, set ALL buttons to blank/empty state (like paging.c does with blank_png)
    // This ensures no stale icons remain from previous page
    for (int i = 1; i <= BUTTON_COUNT; ++i) {
        m_buttons[i - 1]->clearButtonPixmap();
    }
    
    // Step 2: Force immediate update to ensure blank state is visible
    QApplication::processEvents();
    
    // Step 3: Now load new icons for current page
    // Try composed icons first (wp_comp), then fallback to wallpaper tiles
    int iconsLoaded = 0;
    for (int i = 1; i <= BUTTON_COUNT; ++i) {
        // First try composed icon (icon + wallpaper)
        QPixmap pix = m_iconCache->getButtonIcon(i, m_currentPage);
        // Check if pixmap is valid (non-null AND has valid size)
        if (!pix.isNull() && pix.width() > 0 && pix.height() > 0) {
            m_buttons[i - 1]->setButtonPixmap(pix);
            iconsLoaded++;
            qDebug() << "  -> Loaded composed icon for button" << i;
        } else {
            // Fallback to wallpaper tile only (for empty buttons)
            QPixmap wpPix = m_iconCache->getWallpaperTile(i, m_currentPage);
            if (!wpPix.isNull() && wpPix.width() > 0 && wpPix.height() > 0) {
                m_buttons[i - 1]->setButtonPixmap(wpPix);
                iconsLoaded++;
                qDebug() << "  -> Loaded wallpaper tile for button" << i;
            } else {
                qDebug() << "  -> No icon found for button" << i;
            }
        }
    }
    
    qDebug() << "Loaded" << iconsLoaded << "icons for page" << m_currentPage;
}

void DeckWidget::refreshIcons()
{
    loadIconsFromCache();
}

void DeckWidget::setCurrentPage(const QString &pageName)
{
    m_currentPage = pageName;
    loadIconsFromCache();
}

QString DeckWidget::getCurrentPageFromCache() const
{
    if (m_iconCache) {
        return m_iconCache->getCurrentPage();
    }
    return QString();
}

void DeckWidget::updateButtonConfig(int index, const QString &name, const QString &iconPath)
{
    if (index < 1 || index > BUTTON_COUNT) return;
    
    ButtonWidget *btn = m_buttons[index - 1];
    btn->setButtonName(name);
    
    if (!iconPath.isEmpty()) {
        QIcon icon(iconPath);
        btn->setButtonIcon(icon);
    }
}

void DeckWidget::clearButtonConfig(int index)
{
    if (index < 1 || index > BUTTON_COUNT) return;
    
    ButtonWidget *btn = m_buttons[index - 1];
    btn->setButtonName("");
    btn->setButtonIcon(QIcon());
}

void DeckWidget::setButtonPressed(int index, bool pressed)
{
    if (index < 1 || index > BUTTON_COUNT) return;
    m_buttons[index - 1]->setPressed(pressed);
}

void DeckWidget::onButtonTapped(int index)
{
    if (m_client) {
        m_client->simulateButton(index, ButtonEvent::TAP);
    }
    emit buttonTapped(index);
}

void DeckWidget::onButtonHeld(int index)
{
    if (m_client) {
        m_client->simulateButton(index, ButtonEvent::HOLD);
    }
    emit buttonHeld(index);
}

void DeckWidget::onButtonLongHeld(int index)
{
    if (m_client) {
        m_client->simulateButton(index, ButtonEvent::LONGHOLD);
    }
    emit buttonLongHeld(index);
}

void DeckWidget::onButtonReleased(int index)
{
    if (m_client) {
        m_client->simulateButton(index, ButtonEvent::RELEASED);
    }
    emit buttonReleased(index);
}

void DeckWidget::onPageChanged(const QString &pageName, int offset)
{
    qDebug() << "========================================";
    qDebug() << "DeckWidget: Page changed to" << pageName << "offset" << offset;
    qDebug() << "Previous page was:" << m_currentPage;
    qDebug() << "========================================";
    
    // Unsubscribe from old page entities
    if (!m_currentPage.isEmpty()) {
        m_stateManager->unsubscribePageEntities(m_currentPage);
        m_stateManager->clearPage(m_currentPage);
    }
    
    m_currentPage = pageName;
    loadIconsFromCache();
    
    // Request config for new page from MainWindow
    for (int i = 1; i <= BUTTON_COUNT; ++i) {
        emit requestConfigForButton(i, pageName);
    }
    
    // Subscribe to new page entities after a short delay
    QTimer::singleShot(200, [this, pageName]() {
        m_stateManager->subscribePageEntities(pageName);
    });
    
    // Notify MainWindow so it can reload button configs
    emit pageChanged(pageName);
}

void DeckWidget::registerButtonForStateTracking(int buttonIndex, const QString &pageName,
                                                  const QString &entityId, const QStringList &allPresets)
{
    if (!m_stateManager) return;
    
    m_stateManager->registerButton(buttonIndex, pageName, entityId, allPresets);
}

void DeckWidget::setupStateTracking()
{
    if (m_stateTrackingEnabled) return;
    
    m_stateTrackingEnabled = true;
    m_stateManager->startPolling(500);
    
    // Subscribe to entities on current page
    if (!m_currentPage.isEmpty()) {
        m_stateManager->subscribePageEntities(m_currentPage);
    }
    
    // Refresh icons after state tracking is set up (for dynamic text overlays)
    QTimer::singleShot(200, this, &DeckWidget::loadIconsFromCache);
    
    qDebug() << "[DeckWidget] State tracking enabled";
}

void DeckWidget::onButtonStateChanged(int buttonIndex, const QString &pageName, const QString &newState)
{
    if (pageName != m_currentPage) return;
    
    qDebug() << "[DeckWidget] Button" << buttonIndex << "state changed to" << newState;
    
    // Reload icon for this button with the new state
    updateButtonStateIcon(buttonIndex, newState);
}

void DeckWidget::onButtonTextChanged(int buttonIndex, const QString &pageName, const QString &newText)
{
    if (pageName != m_currentPage) return;
    
    qDebug() << "[DeckWidget] Button" << buttonIndex << "text changed to" << newText;
    
    // Update button label with new text (for sensors)
    if (buttonIndex >= 1 && buttonIndex <= BUTTON_COUNT) {
        ButtonWidget *btn = m_buttons[buttonIndex - 1];
        QString currentName = btn->property("originalName").toString();
        if (currentName.isEmpty()) {
            currentName = btn->property("buttonName").toString();
        }
        
        // For sensors, append the value to the name
        if (!newText.isEmpty()) {
            btn->setButtonName(currentName + "\n" + newText);
        } else {
            btn->setButtonName(currentName);
        }
        
        // Also refresh the icon to show text overlay
        refreshButtonIcon(buttonIndex);
    }
}

void DeckWidget::onButtonNeedsRefresh(int buttonIndex, const QString &pageName)
{
    if (pageName != m_currentPage) return;
    
    refreshButtonIcon(buttonIndex);
}

void DeckWidget::onIconUpdated(int buttonIndex, const QString &pageName)
{
    if (pageName != m_currentPage) return;
    if (buttonIndex < 1 || buttonIndex > BUTTON_COUNT) return;
    
    qDebug() << "[DeckWidget] Icon updated for button" << buttonIndex << "page" << pageName;
    refreshButtonIcon(buttonIndex);
}

void DeckWidget::refreshButtonIcon(int index)
{
    if (index < 1 || index > BUTTON_COUNT) return;
    
    loadButtonIcon(index);
}

void DeckWidget::loadButtonIcon(int buttonIndex)
{
    if (!m_iconCache || !m_iconCache->hasSessionCache()) return;
    
    QString iconName = getButtonIconName(buttonIndex);
    QString stateValue = m_stateManager->getButtonState(buttonIndex, m_currentPage);
    QString textValue = m_stateManager->getButtonText(buttonIndex, m_currentPage);
    
    qDebug() << "[DeckWidget] loadButtonIcon" << buttonIndex << "page" << m_currentPage
             << "iconName" << iconName << "state" << stateValue << "text" << textValue;
    
    QPixmap pix;
    
    // FIRST: Check if there's a pre-rendered icon with text in current/ directory
    // This is where the daemon saves sensor values and exec_text results
    pix = m_iconCache->getIconFromCurrent(buttonIndex, m_currentPage);
    if (!pix.isNull()) {
        qDebug() << "[DeckWidget] LOADED icon from current/ for button" << buttonIndex;
        m_buttons[buttonIndex - 1]->setButtonPixmap(pix);
        return;
    }
    
    // Check if this button has state-based icon
    if (!stateValue.isEmpty() && m_stateManager->hasStateBasedIcon(buttonIndex, m_currentPage)) {
        // Try to get state-specific icon
        pix = m_iconCache->getComposedIconForState(buttonIndex, iconName, m_currentPage, stateValue);
        qDebug() << "[DeckWidget] State icon result:" << !pix.isNull();
    }
    
    // Check if this button has dynamic text (sensors, $cmd.exec_text)
    if (pix.isNull() && !textValue.isEmpty()) {
        qDebug() << "[DeckWidget] Trying text icon...";
        pix = m_iconCache->getIconWithText(buttonIndex, iconName, m_currentPage, textValue);
        qDebug() << "[DeckWidget] Text icon result:" << !pix.isNull();
    }
    
    // Fallback to regular icon
    if (pix.isNull()) {
        pix = m_iconCache->getComposedIcon(buttonIndex, iconName, m_currentPage);
    }
    
    // Final fallback to wallpaper tile
    if (pix.isNull() || pix.width() == 0) {
        pix = m_iconCache->getWallpaperTile(buttonIndex, m_currentPage);
    }
    
    if (!pix.isNull() && pix.width() > 0) {
        m_buttons[buttonIndex - 1]->setButtonPixmap(pix);
    }
}

QString DeckWidget::getButtonIconName(int buttonIndex) const
{
    auto it = m_buttonIcons.find(buttonIndex);
    if (it != m_buttonIcons.end()) {
        return it.value();
    }
    return QString();
}

void DeckWidget::updateButtonStateIcon(int buttonIndex, const QString &stateValue)
{
    if (buttonIndex < 1 || buttonIndex > BUTTON_COUNT) return;
    
    QString iconName = getButtonIconName(buttonIndex);
    
    // Try to get state-specific icon
    QPixmap pix = m_iconCache->getComposedIconForState(buttonIndex, iconName, m_currentPage, stateValue);
    
    if (!pix.isNull() && pix.width() > 0) {
        m_buttons[buttonIndex - 1]->setButtonPixmap(pix);
        qDebug() << "[DeckWidget] Updated button" << buttonIndex << "icon for state" << stateValue;
    } else {
        // Fallback: refresh with regular icon
        loadButtonIcon(buttonIndex);
    }
}
