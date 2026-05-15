#include "StateManager.h"
#include "HaClient.h"
#include <QFileSystemWatcher>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QRegularExpression>

StateManager::StateManager(QObject *parent)
    : QObject(parent)
{
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(500);
    connect(m_pollTimer, &QTimer::timeout, this, &StateManager::pollTextUpdates);
    
    // Find session directory
    QDir shmDir("/dev/shm");
    if (shmDir.exists()) {
        QString pagingDir = "/dev/shm/goofydeck/paging";
        if (QDir(pagingDir).exists()) {
            m_sessionDir = pagingDir;
        } else {
            // Try old structure
            QStringList entries = shmDir.entryList(QStringList() << "goofydeck_*", QDir::Dirs);
            if (!entries.isEmpty()) {
                entries.sort();
                QString candidate = shmDir.absoluteFilePath(entries.last()) + "/paging";
                if (QDir(candidate).exists()) {
                    m_sessionDir = candidate;
                }
            }
        }
    }
    
    // Setup file watcher for current directory (where current icons with text are stored)
    m_wpCompWatcher = new QFileSystemWatcher(this);
    connect(m_wpCompWatcher, &QFileSystemWatcher::directoryChanged,
            this, &StateManager::onWpCompChanged);
    connect(m_wpCompWatcher, &QFileSystemWatcher::fileChanged,
            this, &StateManager::onWpCompChanged);
    
    // Watch the current directory if it exists
    if (!m_sessionDir.isEmpty()) {
        QString currentDir = m_sessionDir + "/current";
        if (QDir(currentDir).exists()) {
            m_wpCompWatcher->addPath(currentDir);
            qDebug() << "[StateManager] Watching current directory:" << currentDir;
        } else {
            qDebug() << "[StateManager] current/ directory does not exist yet:" << currentDir;
        }
    }
    
    qDebug() << "[StateManager] Session directory:" << m_sessionDir;
}

StateManager::~StateManager()
{
    stopPolling();
}

void StateManager::setHaClient(HaClient *client)
{
    m_haClient = client;
    if (m_haClient) {
        connect(m_haClient, &HaClient::entityStateChanged,
                this, &StateManager::onHaEntityStateChanged);
    }
}

void StateManager::registerButton(int buttonIndex, const QString &pageName,
                                  const QString &entityId, const QStringList &allPresets)
{
    QString key = generateKey(buttonIndex, pageName);
    
    ButtonState &state = m_buttonStates[key];
    state.buttonIndex = buttonIndex;
    state.pageName = pageName;
    state.entityId = entityId;
    state.allPresets = allPresets;
    state.hasStateConfig = false;
    
    if (!entityId.isEmpty()) {
        m_entityToButtonKey[entityId] = key;
    }
    
    qDebug() << "[StateManager] Registered button" << buttonIndex << "on page" << pageName
             << "entity:" << entityId;
    
    // Check for existing images in wp_comp
    checkForNewButtonImage(buttonIndex, pageName);
}

void StateManager::addStateConfig(int buttonIndex, const QString &pageName,
                                  const QString &stateValue, const StateConfig &config)
{
    QString key = generateKey(buttonIndex, pageName);
    
    if (!m_buttonStates.contains(key)) {
        qDebug() << "[StateManager] Warning: adding state config for unregistered button" << key;
        ButtonState state;
        state.buttonIndex = buttonIndex;
        state.pageName = pageName;
        m_buttonStates[key] = state;
    }
    
    ButtonState &state = m_buttonStates[key];
    state.hasStateConfig = true;
    state.stateConfigs[stateValue] = config;
    
    qDebug() << "[StateManager] Added state config for button" << buttonIndex 
             << "state:" << stateValue << "icon:" << config.icon;
}

void StateManager::unregisterButton(int buttonIndex, const QString &pageName)
{
    QString key = generateKey(buttonIndex, pageName);
    
    if (m_buttonStates.contains(key)) {
        const ButtonState &state = m_buttonStates[key];
        
        // Unregister from tracking
        if (!state.entityId.isEmpty() && m_haClient) {
            m_haClient->unsubscribeEntity(state.entityId);
            m_entityToButtonKey.remove(state.entityId);
        }
        
        m_buttonStates.remove(key);
    }
}

void StateManager::clearPage(const QString &pageName)
{
    // Find all buttons on this page
    QStringList keysToRemove;
    for (auto it = m_buttonStates.begin(); it != m_buttonStates.end(); ++it) {
        if (it.value().pageName == pageName) {
            keysToRemove.append(it.key());
        }
    }
    
    // Unsubscribe all
    unsubscribePageEntities(pageName);
    
    // Remove all buttons
    for (const QString &key : keysToRemove) {
        ButtonState state = m_buttonStates.take(key);
        (void)state; // Unused
    }
    
    qDebug() << "[StateManager] Cleared page" << pageName << "removed" << keysToRemove.size() << "buttons";
}

QString StateManager::getButtonState(int buttonIndex, const QString &pageName) const
{
    QString key = generateKey(buttonIndex, pageName);
    auto it = m_buttonStates.find(key);
    if (it != m_buttonStates.end()) {
        return it->currentState;
    }
    return QString();
}

QString StateManager::getButtonText(int buttonIndex, const QString &pageName) const
{
    QString key = generateKey(buttonIndex, pageName);
    auto it = m_buttonStates.find(key);
    if (it != m_buttonStates.end()) {
        return it->currentText;
    }
    return QString();
}

StateConfig StateManager::getStateConfig(int buttonIndex, const QString &pageName,
                                         const QString &stateValue) const
{
    QString key = generateKey(buttonIndex, pageName);
    auto it = m_buttonStates.find(key);
    if (it != m_buttonStates.end()) {
        auto configIt = it->stateConfigs.find(stateValue);
        if (configIt != it->stateConfigs.end()) {
            return configIt.value();
        }
    }
    return StateConfig();
}

bool StateManager::hasStateBasedIcon(int buttonIndex, const QString &pageName) const
{
    QString key = generateKey(buttonIndex, pageName);
    auto it = m_buttonStates.find(key);
    if (it != m_buttonStates.end()) {
        return it->hasStateConfig && !it->stateConfigs.isEmpty();
    }
    return false;
}

bool StateManager::hasDynamicText(int buttonIndex, const QString &pageName) const
{
    // Check if there's a text file for this button
    QString textPath = getTextFilePath(buttonIndex, pageName);
    if (!textPath.isEmpty() && QFile::exists(textPath)) {
        return true;
    }
    
    // Or check if button has entity_id (sensor values display as text)
    QString key = generateKey(buttonIndex, pageName);
    auto it = m_buttonStates.find(key);
    if (it != m_buttonStates.end()) {
        // If entity starts with sensor. or has no state configs, it likely displays text
        if (it->entityId.startsWith("sensor.")) {
            return true;
        }
    }
    
    return false;
}

void StateManager::subscribePageEntities(const QString &pageName)
{
    m_currentPage = pageName;
    
    if (!m_haClient || !m_haClient->isConfigured()) {
        qDebug() << "[StateManager] HA not configured, skipping subscription";
        return;
    }
    
    for (auto it = m_buttonStates.begin(); it != m_buttonStates.end(); ++it) {
        const ButtonState &state = it.value();
        if (state.pageName == pageName && !state.entityId.isEmpty()) {
            // Only subscribe to real HA entities (containing a dot like sensor.xxx, light.xxx)
            // Skip page names and other non-entity data
            if (isValidHaEntity(state.entityId)) {
                m_haClient->subscribeEntity(state.entityId);
                
                // Also request current state
                m_haClient->requestState(state.entityId);
            } else {
                qDebug() << "[StateManager] Skipping non-HA entity:" << state.entityId;
            }
        }
    }
}

bool StateManager::isValidHaEntity(const QString &entityId) const
{
    // Valid HA entities have format: domain.entity_name (e.g., sensor.temperature, light.salon)
    // Must contain exactly one dot and no spaces
    if (entityId.isEmpty()) return false;
    
    // Check for domain format: xxx.yyy
    int dotCount = entityId.count('.');
    if (dotCount != 1) return false;
    
    // Common HA domains
    QStringList validDomains = {
        "sensor", "binary_sensor", "switch", "light", "climate", 
        "fan", "cover", "lock", "vacuum", "weather", "person",
        "device_tracker", "sun", "media_player", "input_boolean",
        "input_number", "input_text", "automation", "script"
    };
    
    QString domain = entityId.split('.').first();
    return validDomains.contains(domain);
}

void StateManager::unsubscribePageEntities(const QString &pageName)
{
    if (!m_haClient) return;
    
    for (auto it = m_buttonStates.begin(); it != m_buttonStates.end(); ++it) {
        const ButtonState &state = it.value();
        if (state.pageName == pageName && !state.entityId.isEmpty()) {
            // Only unsubscribe real HA entities
            if (isValidHaEntity(state.entityId)) {
                m_haClient->unsubscribeEntity(state.entityId);
            }
        }
    }
}

void StateManager::startPolling(int intervalMs)
{
    m_pollTimer->setInterval(intervalMs);
    m_pollTimer->start();
    qDebug() << "[StateManager] Started polling with interval" << intervalMs << "ms";
}

void StateManager::stopPolling()
{
    m_pollTimer->stop();
}

void StateManager::onHaEntityStateChanged(const QString &entityId, const QString &state, const QString &unit)
{
    auto it = m_entityToButtonKey.find(entityId);
    if (it == m_entityToButtonKey.end()) {
        return; // Not tracking this entity
    }
    
    QString key = it.value();
    auto btnIt = m_buttonStates.find(key);
    if (btnIt == m_buttonStates.end()) {
        return;
    }
    
    ButtonState &btnState = btnIt.value();
    QString oldState = btnState.currentState;
    btnState.currentState = state;
    
    // Build display text (value + unit for sensors)
    QString displayText = state;
    if (!unit.isEmpty()) {
        displayText += " " + unit;
    }
    btnState.currentText = displayText;
    
    qDebug() << "[StateManager] Button" << btnState.buttonIndex << "state changed:" 
             << oldState << "->" << state;
    
    // Check if state-based icon should change
    if (btnState.hasStateConfig && btnState.stateConfigs.contains(state)) {
        emit buttonStateChanged(btnState.buttonIndex, btnState.pageName, state);
        emit buttonNeedsRefresh(btnState.buttonIndex, btnState.pageName);
    } else if (btnState.entityId.startsWith("sensor.")) {
        // For sensors, text changed
        emit buttonTextChanged(btnState.buttonIndex, btnState.pageName, displayText);
    } else if (oldState != state) {
        // State changed but no specific config - still refresh
        emit buttonStateChanged(btnState.buttonIndex, btnState.pageName, state);
    }
}

void StateManager::onWpCompChanged()
{
    // WP Comp directory changed - a new composed icon might have been created
    // Check all tracked buttons for their current page
    for (auto it = m_buttonStates.begin(); it != m_buttonStates.end(); ++it) {
        const ButtonState &state = it.value();
        // Only check buttons on current page
        if (state.pageName != m_currentPage) continue;
        
        // Check if there's a new image for this button
        checkForNewButtonImage(state.buttonIndex, state.pageName);
    }
}

void StateManager::checkForNewButtonImage(int buttonIndex, const QString &pageName)
{
    if (m_sessionDir.isEmpty()) return;
    
    // Look in current/ directory for the latest icon with text
    QString currentDir = m_sessionDir + "/current";
    QDir cdir(currentDir);
    if (!cdir.exists()) return;
    
    // Format: <page>_<pos>.png (e.g., lumiere_06.png)
    QString pageTag = pageName;
    // Remove $ prefix if present for $root page
    if (pageTag.startsWith("$")) pageTag = pageTag.mid(1);
    // Sanitize like paging.c does
    pageTag.replace(QRegularExpression("[^a-zA-Z0-9_]"), "_");
    
    QString fileName = QString("%1_%2.png").arg(pageTag).arg(buttonIndex, 2, 10, QChar('0'));
    QString filePath = currentDir + "/" + fileName;
    
    QFileInfo info(filePath);
    if (!info.exists()) return;
    
    QString key = generateKey(buttonIndex, pageName);
    QString lastKey = key + ":image";
    QString oldImage = m_lastImage.value(lastKey);
    QDateTime lastTime = m_lastImageTime.value(lastKey);
    
    // Check if file changed (different path or newer modified time)
    if (filePath != oldImage || info.lastModified() > lastTime) {
        m_lastImage[lastKey] = filePath;
        m_lastImageTime[lastKey] = info.lastModified();
        
        qDebug() << "[StateManager] New/updated image for button" << buttonIndex << ":" << filePath;
        emit buttonNeedsRefresh(buttonIndex, pageName);
    }
}

void StateManager::pollTextUpdates()
{
    // Poll for new images in current/ (for $cmd.exec_text, sensor updates, and state changes)
    
    // Check if current/ directory was created and start watching it
    QString currentDir = m_sessionDir + "/current";
    if (!m_wpCompWatcher->directories().contains(currentDir)) {
        if (QDir(currentDir).exists()) {
            m_wpCompWatcher->addPath(currentDir);
            qDebug() << "[StateManager] Now watching current directory:" << currentDir;
        }
    }
    
    for (auto it = m_buttonStates.begin(); it != m_buttonStates.end(); ++it) {
        const ButtonState &state = it.value();
        if (state.pageName == m_currentPage) {
            checkForNewButtonImage(state.buttonIndex, state.pageName);
        }
    }
}

QString StateManager::getTextFilePath(int buttonIndex, const QString &pageName) const
{
    if (m_sessionDir.isEmpty()) return QString();
    
    // Text files are stored as: <session_dir>/text/<page_name>/<button>.txt
    QString path = QString("%1/text/%2/%3.txt").arg(m_sessionDir).arg(pageName).arg(buttonIndex);
    return path;
}

QString StateManager::getStateFilePath(int buttonIndex, const QString &pageName) const
{
    if (m_sessionDir.isEmpty()) return QString();
    
    // State files are stored as: <session_dir>/state/<page_name>/<button>.txt
    QString path = QString("%1/state/%2/%3.txt").arg(m_sessionDir).arg(pageName).arg(buttonIndex);
    return path;
}

void StateManager::loadTextFromFile(int buttonIndex, const QString &pageName)
{
    QString textPath = getTextFilePath(buttonIndex, pageName);
    if (textPath.isEmpty()) return;
    
    QString key = generateKey(buttonIndex, pageName);
    
    QFile file(textPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    
    QString text = QString::fromUtf8(file.readAll()).trimmed();
    file.close();
    
    // Check if text changed
    QString lastKey = key + ":text";
    QString oldText = m_lastText.value(lastKey);
    
    if (text != oldText) {
        m_lastText[lastKey] = text;
        
        // Update button state
        auto it = m_buttonStates.find(key);
        if (it != m_buttonStates.end()) {
            it->currentText = text;
        }
        
        qDebug() << "[StateManager] Button" << buttonIndex << "text changed:" << text;
        emit buttonTextChanged(buttonIndex, pageName, text);
    }
}

QString StateManager::generateKey(int buttonIndex, const QString &pageName) const
{
    return QString("%1:%2").arg(buttonIndex).arg(pageName);
}

void StateManager::parseKey(const QString &key, int &buttonIndex, QString &pageName) const
{
    int sepPos = key.indexOf(':');
    if (sepPos > 0) {
        buttonIndex = key.left(sepPos).toInt();
        pageName = key.mid(sepPos + 1);
    }
}
