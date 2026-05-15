#ifndef HACLIENT_H
#define HACLIENT_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>

class QLocalSocket;
class QTimer;

struct HaState {
    QString entityId;
    QString state;
    QString unit;
    QString lastUpdated;
};

class HaClient : public QObject
{
    Q_OBJECT

public:
    explicit HaClient(QObject *parent = nullptr);
    ~HaClient();

    // Load HA config from .env file
    bool loadConfig(const QString &envPath = QString());
    
    // Check if HA is configured and available
    bool isConfigured() const;
    bool isConnected() const;
    
    // Connect to ha_daemon socket
    bool connectToDaemon();
    void disconnectFromDaemon();
    
    // Subscribe/unsubscribe to entity state changes
    bool subscribeEntity(const QString &entityId);
    bool unsubscribeEntity(int subId);
    bool unsubscribeEntity(const QString &entityId);
    
    // Get current state of an entity
    bool requestState(const QString &entityId);
    
    // Get cached state
    QString getEntityState(const QString &entityId) const;
    bool hasEntityState(const QString &entityId) const;

signals:
    void connected();
    void disconnected();
    void entityStateChanged(const QString &entityId, const QString &state, const QString &unit);
    void entityStateReceived(const QString &entityId, const QString &stateJson);
    void error(const QString &error);

private slots:
    void onSocketReadyRead();
    void onSocketError();
    void onSocketDisconnected();
    void reconnect();

private:
    void parseLine(const QByteArray &line);
    int getNextSubId();
    
    QLocalSocket *m_socket = nullptr;
    QTimer *m_reconnectTimer = nullptr;
    
    QString m_haHost;
    QString m_haToken;
    QString m_socketPath;
    
    bool m_configured = false;
    bool m_connected = false;
    int m_nextSubId = 1;
    
    QMap<QString, int> m_entityToSubId;  // entity_id -> sub_id
    QMap<QString, HaState> m_cachedStates;
    QByteArray m_readBuffer;
};

#endif // HACLIENT_H
