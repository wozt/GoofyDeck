#ifndef GOOFYDECKCLIENT_H
#define GOOFYDECKCLIENT_H

#include <QObject>
#include <QString>

struct ButtonEvent {
    int button;
    enum Type { TAP, HOLD, LONGHOLD, RELEASED, UNKNOWN } type;
    QString toString() const;
};

class GoofyDeckClient : public QObject
{
    Q_OBJECT

public:
    explicit GoofyDeckClient(QObject *parent = nullptr);
    ~GoofyDeckClient();

    // Check daemon availability (ping)
    bool ping() const;
    
    // Simple blocking command send with response
    bool sendCommand(const QString &command, QString *response = nullptr, int timeoutMs = 1000);

    // Simulation commands
    bool simulateButton(int button, ButtonEvent::Type event);
    
    // Control commands
    bool stopControl();
    bool startControl();
    bool loadLastPage();
    bool reloadConfig();

    QString lastError() const { return m_lastError; }
    bool isDaemonAvailable() const;

signals:
    void daemonStatusChanged(bool online);
    void errorOccurred(const QString &error);

private:
    QString m_lastError;
    QString m_socketPath;
};

#endif // GOOFYDECKCLIENT_H
