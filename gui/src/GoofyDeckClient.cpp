#include "GoofyDeckClient.h"
#include "Config.h"
#include <QLocalSocket>
#include <QDebug>

QString ButtonEvent::toString() const
{
    switch (type) {
        case TAP: return QString("TAP%1").arg(button);
        case HOLD: return QString("HOLD%1").arg(button);
        case LONGHOLD: return QString("LONGHOLD%1").arg(button);
        case RELEASED: return QString("RELEASED%1").arg(button);
        default: return QString("UNKNOWN%1").arg(button);
    }
}

GoofyDeckClient::GoofyDeckClient(QObject *parent)
    : QObject(parent)
    , m_socketPath(Config::instance().pagingSocketPath())
{
}

GoofyDeckClient::~GoofyDeckClient() = default;

bool GoofyDeckClient::isDaemonAvailable() const
{
    return ping();
}

bool GoofyDeckClient::ping() const
{
    QString resp;
    return const_cast<GoofyDeckClient*>(this)->sendCommand("ping", &resp, 500);
}

bool GoofyDeckClient::sendCommand(const QString &command, QString *response, int timeoutMs)
{
    if (timeoutMs <= 0) {
        timeoutMs = Config::instance().socketTimeoutMs();
    }
    
    QLocalSocket socket;
    socket.connectToServer(m_socketPath);
    
    if (!socket.waitForConnected(timeoutMs)) {
        m_lastError = socket.errorString();
        return false;
    }
    
    // Send command
    QByteArray data = command.toUtf8();
    if (!data.endsWith('\n')) data.append('\n');
    socket.write(data);
    socket.flush();
    
    // Wait for response
    if (!socket.waitForReadyRead(timeoutMs)) {
        m_lastError = "Timeout waiting for response";
        return false;
    }
    
    QByteArray respData = socket.readAll();
    socket.disconnectFromServer();
    
    if (response) {
        *response = QString::fromUtf8(respData).trimmed();
    }
    
    return true;
}

bool GoofyDeckClient::simulateButton(int button, ButtonEvent::Type event)
{
    QString eventStr;
    switch (event) {
        case ButtonEvent::TAP: eventStr = "TAP"; break;
        case ButtonEvent::HOLD: eventStr = "HOLD"; break;
        case ButtonEvent::LONGHOLD: eventStr = "LONGHOLD"; break;
        case ButtonEvent::RELEASED: eventStr = "RELEASED"; break;
        default: return false;
    }
    
    QString cmd = QString("simule-button %1%2").arg(eventStr).arg(button);
    return sendCommand(cmd);
}

bool GoofyDeckClient::stopControl()
{
    return sendCommand("stop-control");
}

bool GoofyDeckClient::startControl()
{
    return sendCommand("start-control");
}

bool GoofyDeckClient::loadLastPage()
{
    return sendCommand("load-last-page");
}

bool GoofyDeckClient::reloadConfig()
{
    return sendCommand("reload-config");
}
