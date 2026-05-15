#include "HaClient.h"
#include <QLocalSocket>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

HaClient::HaClient(QObject *parent)
    : QObject(parent)
    , m_socketPath("/tmp/goofydeck_ha.sock")
{
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setInterval(5000);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &HaClient::reconnect);
}

HaClient::~HaClient()
{
    disconnectFromDaemon();
}

bool HaClient::loadConfig(const QString &envPath)
{
    QString path = envPath;
    if (path.isEmpty()) {
        // Try to find .env in parent directories
        QStringList possiblePaths;
        possiblePaths << "../.env" << "../../.env" << ".env";
        for (const QString &p : possiblePaths) {
            if (QFile::exists(p)) {
                path = p;
                break;
            }
        }
    }
    
    if (path.isEmpty() || !QFile::exists(path)) {
        qDebug() << "[HaClient] .env file not found";
        return false;
    }
    
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "[HaClient] Failed to open .env:" << path;
        return false;
    }
    
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        
        int eqPos = line.indexOf('=');
        if (eqPos <= 0) continue;
        
        QString key = line.left(eqPos).trimmed();
        QString val = line.mid(eqPos + 1).trimmed();
        
        // Remove quotes if present
        if ((val.startsWith('"') && val.endsWith('"')) ||
            (val.startsWith('\'') && val.endsWith('\''))) {
            val = val.mid(1, val.length() - 2);
        }
        
        if (key == "HA_HOST") {
            m_haHost = val;
        } else if (key == "HA_ACCESS_TOKEN") {
            m_haToken = val;
        }
    }
    
    file.close();
    
    m_configured = !m_haHost.isEmpty() && !m_haToken.isEmpty();
    
    if (m_configured) {
        qDebug() << "[HaClient] Config loaded - HA host:" << m_haHost;
    } else {
        qDebug() << "[HaClient] Config incomplete - host:" << !m_haHost.isEmpty() 
                 << "token:" << !m_haToken.isEmpty();
    }
    
    return m_configured;
}

bool HaClient::isConfigured() const
{
    return m_configured;
}

bool HaClient::isConnected() const
{
    return m_connected;
}

bool HaClient::connectToDaemon()
{
    if (!m_configured) {
        if (!loadConfig()) {
            emit error("HA not configured (no .env file)");
            return false;
        }
    }
    
    if (m_socket) {
        disconnectFromDaemon();
    }
    
    m_socket = new QLocalSocket(this);
    connect(m_socket, &QLocalSocket::readyRead, this, &HaClient::onSocketReadyRead);
    connect(m_socket, &QLocalSocket::errorOccurred, this, &HaClient::onSocketError);
    connect(m_socket, &QLocalSocket::disconnected, this, &HaClient::onSocketDisconnected);
    
    m_socket->connectToServer(m_socketPath);
    if (!m_socket->waitForConnected(1000)) {
        qDebug() << "[HaClient] Failed to connect to ha_daemon:" << m_socket->errorString();
        delete m_socket;
        m_socket = nullptr;
        m_reconnectTimer->start();
        return false;
    }
    
    m_connected = true;
    emit connected();
    qDebug() << "[HaClient] Connected to ha_daemon";
    
    // Send ping to verify connection
    m_socket->write("ping\n");
    m_socket->flush();
    
    return true;
}

void HaClient::disconnectFromDaemon()
{
    m_reconnectTimer->stop();
    
    if (m_socket) {
        m_socket->disconnectFromServer();
        delete m_socket;
        m_socket = nullptr;
    }
    
    m_connected = false;
    m_entityToSubId.clear();
    emit disconnected();
}

bool HaClient::subscribeEntity(const QString &entityId)
{
    if (!m_connected || !m_socket) {
        if (!connectToDaemon()) {
            return false;
        }
    }
    
    if (m_entityToSubId.contains(entityId)) {
        qDebug() << "[HaClient] Already subscribed to" << entityId;
        return true; // Already subscribed
    }
    
    int subId = getNextSubId();
    QString cmd = QString("sub-state %1\n").arg(entityId);
    
    qint64 written = m_socket->write(cmd.toUtf8());
    m_socket->flush();
    
    if (written > 0) {
        m_entityToSubId[entityId] = subId;
        qDebug() << "[HaClient] Subscribed to" << entityId << "with sub_id" << subId;
        return true;
    }
    
    return false;
}

bool HaClient::unsubscribeEntity(int subId)
{
    if (!m_connected || !m_socket) return false;
    
    QString cmd = QString("unsub %1\n").arg(subId);
    qint64 written = m_socket->write(cmd.toUtf8());
    m_socket->flush();
    
    // Remove from tracking
    for (auto it = m_entityToSubId.begin(); it != m_entityToSubId.end(); ++it) {
        if (it.value() == subId) {
            m_entityToSubId.erase(it);
            break;
        }
    }
    
    return written > 0;
}

bool HaClient::unsubscribeEntity(const QString &entityId)
{
    if (!m_entityToSubId.contains(entityId)) return true;
    
    int subId = m_entityToSubId[entityId];
    return unsubscribeEntity(subId);
}

bool HaClient::requestState(const QString &entityId)
{
    if (!m_connected || !m_socket) {
        if (!connectToDaemon()) {
            return false;
        }
    }
    
    QString cmd = QString("get %1\n").arg(entityId);
    qint64 written = m_socket->write(cmd.toUtf8());
    m_socket->flush();
    
    return written > 0;
}

QString HaClient::getEntityState(const QString &entityId) const
{
    auto it = m_cachedStates.find(entityId);
    if (it != m_cachedStates.end()) {
        return it->state;
    }
    return QString();
}

bool HaClient::hasEntityState(const QString &entityId) const
{
    return m_cachedStates.contains(entityId);
}

void HaClient::onSocketReadyRead()
{
    while (m_socket->canReadLine() || m_socket->bytesAvailable() > 0) {
        QByteArray line = m_socket->readLine();
        if (!line.isEmpty()) {
            parseLine(line);
        }
    }
}

void HaClient::parseLine(const QByteArray &line)
{
    QString msg = QString::fromUtf8(line).trimmed();
    
    if (msg == "ok") {
        // Generic OK response
        return;
    }
    
    if (msg == "evt connected") {
        qDebug() << "[HaClient] HA WebSocket connected";
        // Re-subscribe to all entities
        QStringList entities = m_entityToSubId.keys();
        m_entityToSubId.clear();
        for (const QString &entity : entities) {
            subscribeEntity(entity);
        }
        return;
    }
    
    if (msg == "evt disconnected") {
        qDebug() << "[HaClient] HA WebSocket disconnected";
        return;
    }
    
    if (msg.startsWith("ok sub_id=")) {
        // Subscription confirmation
        int subId = msg.mid(10).toInt();
        qDebug() << "[HaClient] Subscription confirmed, sub_id=" << subId;
        return;
    }
    
    if (msg.startsWith("evt state ")) {
        // State update: "evt state <entity_id> <json>"
        int firstSpace = msg.indexOf(' ', 10);
        if (firstSpace > 0) {
            QString entityId = msg.mid(10, firstSpace - 10);
            QString jsonStr = msg.mid(firstSpace + 1);
            
            // Parse JSON state
            QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
            if (!doc.isNull() && doc.isObject()) {
                QJsonObject obj = doc.object();
                QString state = obj.value("state").toString();
                QString unit = obj.value("attributes").toObject().value("unit_of_measurement").toString();
                QString lastUpdated = obj.value("last_updated").toString();
                
                HaState haState;
                haState.entityId = entityId;
                haState.state = state;
                haState.unit = unit;
                haState.lastUpdated = lastUpdated;
                
                m_cachedStates[entityId] = haState;
                
                qDebug() << "[HaClient] State update:" << entityId << "=" << state << unit;
                emit entityStateChanged(entityId, state, unit);
                emit entityStateReceived(entityId, jsonStr);
            }
        }
        return;
    }
    
    if (msg.startsWith("ok ")) {
        // Response to get command: "ok {<state_json>}"
        QString jsonStr = msg.mid(3);
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject obj = doc.object();
            QString entityId = obj.value("entity_id").toString();
            
            if (!entityId.isEmpty()) {
                QString state = obj.value("state").toString();
                QString unit = obj.value("attributes").toObject().value("unit_of_measurement").toString();
                
                HaState haState;
                haState.entityId = entityId;
                haState.state = state;
                haState.unit = unit;
                m_cachedStates[entityId] = haState;
                
                emit entityStateReceived(entityId, jsonStr);
            }
        }
        return;
    }
    
    if (msg.startsWith("err ")) {
        qDebug() << "[HaClient] Error from ha_daemon:" << msg;
        emit error(msg.mid(4));
        return;
    }
    
    qDebug() << "[HaClient] Unknown message:" << msg;
}

void HaClient::onSocketError()
{
    if (m_socket) {
        qDebug() << "[HaClient] Socket error:" << m_socket->errorString();
    }
    m_connected = false;
    emit disconnected();
    m_reconnectTimer->start();
}

void HaClient::onSocketDisconnected()
{
    qDebug() << "[HaClient] Socket disconnected";
    m_connected = false;
    emit disconnected();
    m_reconnectTimer->start();
}

void HaClient::reconnect()
{
    if (!m_connected) {
        qDebug() << "[HaClient] Attempting reconnect...";
        connectToDaemon();
    }
}

int HaClient::getNextSubId()
{
    return m_nextSubId++;
}
