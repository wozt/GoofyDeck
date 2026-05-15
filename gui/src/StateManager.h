#ifndef STATEMANAGER_H
#define STATEMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QSet>
#include <QTimer>
#include <QDateTime>
#include "YamlParser.h"  // For StateConfig struct

class HaClient;
class QFileSystemWatcher;
// Button state tracking
struct ButtonState {
    int buttonIndex = -1;
    QString pageName;
    QString entityId;       // HA entity_id or state identifier
    QString currentState;   // Current state value ("on", "off", etc.)
    QString currentText;    // Current displayed text (for sensors)
    bool hasStateConfig = false;
    QMap<QString, StateConfig> stateConfigs;  // state value -> config
    QStringList allPresets; // Base presets for the button
};

class StateManager : public QObject
{
    Q_OBJECT

public:
    explicit StateManager(QObject *parent = nullptr);
    ~StateManager();

    void setHaClient(HaClient *client);
    
    // Register a button for state tracking
    void registerButton(int buttonIndex, const QString &pageName, 
                        const QString &entityId, const QStringList &allPresets);
    
    // Add state configuration for a button
    void addStateConfig(int buttonIndex, const QString &pageName,
                        const QString &stateValue, const StateConfig &config);
    
    // Remove a button from tracking
    void unregisterButton(int buttonIndex, const QString &pageName);
    
    // Clear all tracking for a page
    void clearPage(const QString &pageName);
    
    // Get current state info
    QString getButtonState(int buttonIndex, const QString &pageName) const;
    QString getButtonText(int buttonIndex, const QString &pageName) const;
    StateConfig getStateConfig(int buttonIndex, const QString &pageName, 
                               const QString &stateValue) const;
    
    // Check if button has state-based icon
    bool hasStateBasedIcon(int buttonIndex, const QString &pageName) const;
    bool hasDynamicText(int buttonIndex, const QString &pageName) const;
    
    // Subscribe to all entities on current page
    void subscribePageEntities(const QString &pageName);
    void unsubscribePageEntities(const QString &pageName);

    // Start/stop state polling
    void startPolling(int intervalMs = 500);
    void stopPolling();

signals:
    // Emitted when a button's state or text changes
    void buttonStateChanged(int buttonIndex, const QString &pageName, 
                            const QString &newState);
    void buttonTextChanged(int buttonIndex, const QString &pageName,
                           const QString &newText);
    void buttonNeedsRefresh(int buttonIndex, const QString &pageName);

private slots:
    void onHaEntityStateChanged(const QString &entityId, const QString &state, const QString &unit);
    void onWpCompChanged();
    void pollTextUpdates();

private:
    QString getTextFilePath(int buttonIndex, const QString &pageName) const;
    QString getStateFilePath(int buttonIndex, const QString &pageName) const;
    void loadTextFromFile(int buttonIndex, const QString &pageName);
    QString generateKey(int buttonIndex, const QString &pageName) const;
    void parseKey(const QString &key, int &buttonIndex, QString &pageName) const;
    void checkForNewButtonImage(int buttonIndex, const QString &pageName);
    bool isValidHaEntity(const QString &entityId) const;

    HaClient *m_haClient = nullptr;
    QMap<QString, ButtonState> m_buttonStates;  // key (btn:page) -> state
    QMap<QString, QString> m_entityToButtonKey; // entity_id -> button key
    
    // WP Comp directory monitoring (where composed icons with text are stored)
    QFileSystemWatcher *m_wpCompWatcher = nullptr;
    QTimer *m_pollTimer = nullptr;
    QString m_sessionDir;
    
    // Last known text/state to detect changes
    QMap<QString, QString> m_lastText;
    QMap<QString, QString> m_lastState;
    QMap<QString, QString> m_lastImage;  // Track last image path for each button
    QMap<QString, QDateTime> m_lastImageTime;  // Track last modified time
    QString m_currentPage;
};

#endif // STATEMANAGER_H
