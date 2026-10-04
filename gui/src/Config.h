#ifndef CONFIG_H
#define CONFIG_H

#include <QString>
#include <QSettings>
#include <QDir>

class Config
{
public:
    static Config& instance();
    
    // Path getters with fallbacks
    QString configPath() const;
    QString pagingSocketPath() const;
    QString haSocketPath() const;
    QString cacheRoot() const;
    QString sessionDir() const;
    QString envFilePath() const;
    QString assetsPath() const;
    
    // Configuration setters
    void setConfigPath(const QString &path);
    void setPagingSocketPath(const QString &path);
    void setHaSocketPath(const QString &path);
    void setCacheRoot(const QString &path);
    
    // Timeout settings
    int socketTimeoutMs() const;
    void setSocketTimeoutMs(int timeout);
    
    // Polling settings
    int pollingIntervalMs() const;
    void setPollingIntervalMs(int interval);
    
    // UI settings
    bool useFixedSize() const;
    void setUseFixedSize(bool fixed);
    
    // Logging settings
    bool enableDebugLogging() const;
    void setEnableDebugLogging(bool enable);
    
    // Load from environment or settings
    void loadFromEnvironment();
    void loadFromSettings();
    void saveToSettings();
    
private:
    Config();
    ~Config();
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
    QString findConfigPath() const;
    QString findEnvFile() const;
    
    QString m_configPath;
    QString m_pagingSocketPath;
    QString m_haSocketPath;
    QString m_cacheRoot;
    QString m_sessionDir;
    QString m_envFilePath;
    QString m_assetsPath;
    
    int m_socketTimeoutMs;
    int m_pollingIntervalMs;
    bool m_useFixedSize;
    bool m_enableDebugLogging;
    
    QSettings m_settings;
};

#endif // CONFIG_H
