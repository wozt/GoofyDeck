#include "Config.h"
#include <QStandardPaths>
#include <QFileInfo>
#include <QDebug>
#include <QCoreApplication>

Config::Config()
    : m_settings("goofydeck", "goofydeck-gui")
    , m_socketTimeoutMs(5000)
    , m_pollingIntervalMs(1000)
    , m_useFixedSize(false)
    , m_enableDebugLogging(false)
{
    // Set default paths
    m_pagingSocketPath = "/tmp/goofydeck_paging_control.sock";
    m_haSocketPath = "/tmp/goofydeck_ha.sock";
    m_cacheRoot = ".cache";
    
    // Load from settings first
    loadFromSettings();
    
    // Then override with environment if available
    loadFromEnvironment();
    
    // Find config and env files if not set
    if (m_configPath.isEmpty()) {
        m_configPath = findConfigPath();
    }
    if (m_envFilePath.isEmpty()) {
        m_envFilePath = findEnvFile();
    }
    
    // Set session dir based on cache root
    m_sessionDir = "/dev/shm/goofydeck/paging";
    
    // Set assets path relative to config
    QFileInfo configInfo(m_configPath);
    if (configInfo.exists()) {
        QDir configDir = configInfo.absoluteDir();
        m_assetsPath = configDir.absoluteFilePath("../assets");
    } else {
        m_assetsPath = "../assets";
    }
}

Config::~Config()
{
    saveToSettings();
}

Config& Config::instance()
{
    static Config instance;
    return instance;
}

QString Config::findConfigPath() const
{
    QStringList possiblePaths;
    
    // Try relative paths from current directory
    possiblePaths << "../config/configuration.yml";
    possiblePaths << "../../config/configuration.yml";
    possiblePaths << "./config/configuration.yml";
    
    // Try home directory
    possiblePaths << QDir::home().filePath("GoofyDeck/config/configuration.yml");
    
    // Try XDG config directory
    QString xdgConfig = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (!xdgConfig.isEmpty()) {
        possiblePaths << xdgConfig + "/configuration.yml";
    }
    
    for (const QString &path : possiblePaths) {
        if (QFile::exists(path)) {
            return path;
        }
    }
    
    // Return first possible path as default even if it doesn't exist
    return possiblePaths.first();
}

QString Config::findEnvFile() const
{
    QStringList possiblePaths;
    
    // Try relative paths
    possiblePaths << "../.env";
    possiblePaths << "../../.env";
    possiblePaths << "./.env";
    
    // Try home directory
    possiblePaths << QDir::home().filePath("GoofyDeck/.env");
    
    // Try XDG config directory
    QString xdgConfig = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (!xdgConfig.isEmpty()) {
        possiblePaths << xdgConfig + "/.env";
    }
    
    for (const QString &path : possiblePaths) {
        if (QFile::exists(path)) {
            return path;
        }
    }
    
    return QString();
}

QString Config::configPath() const
{
    return m_configPath;
}

QString Config::pagingSocketPath() const
{
    return m_pagingSocketPath;
}

QString Config::haSocketPath() const
{
    return m_haSocketPath;
}

QString Config::cacheRoot() const
{
    return m_cacheRoot;
}

QString Config::sessionDir() const
{
    return m_sessionDir;
}

QString Config::envFilePath() const
{
    return m_envFilePath;
}

QString Config::assetsPath() const
{
    return m_assetsPath;
}

void Config::setConfigPath(const QString &path)
{
    m_configPath = path;
}

void Config::setPagingSocketPath(const QString &path)
{
    m_pagingSocketPath = path;
}

void Config::setHaSocketPath(const QString &path)
{
    m_haSocketPath = path;
}

void Config::setCacheRoot(const QString &path)
{
    m_cacheRoot = path;
}

int Config::socketTimeoutMs() const
{
    return m_socketTimeoutMs;
}

void Config::setSocketTimeoutMs(int timeout)
{
    m_socketTimeoutMs = timeout;
}

int Config::pollingIntervalMs() const
{
    return m_pollingIntervalMs;
}

void Config::setPollingIntervalMs(int interval)
{
    m_pollingIntervalMs = interval;
}

bool Config::useFixedSize() const
{
    return m_useFixedSize;
}

void Config::setUseFixedSize(bool fixed)
{
    m_useFixedSize = fixed;
}

bool Config::enableDebugLogging() const
{
    return m_enableDebugLogging;
}

void Config::setEnableDebugLogging(bool enable)
{
    m_enableDebugLogging = enable;
}

void Config::loadFromEnvironment()
{
    // Check environment variables
    if (qEnvironmentVariableIsSet("GOOFYDECK_CONFIG")) {
        m_configPath = qEnvironmentVariable("GOOFYDECK_CONFIG");
    }
    
    if (qEnvironmentVariableIsSet("GOOFYDECK_PAGING_SOCKET")) {
        m_pagingSocketPath = qEnvironmentVariable("GOOFYDECK_PAGING_SOCKET");
    }
    
    if (qEnvironmentVariableIsSet("GOOFYDECK_HA_SOCKET")) {
        m_haSocketPath = qEnvironmentVariable("GOOFYDECK_HA_SOCKET");
    }
    
    if (qEnvironmentVariableIsSet("GOOFYDECK_CACHE_ROOT")) {
        m_cacheRoot = qEnvironmentVariable("GOOFYDECK_CACHE_ROOT");
    }
    
    if (qEnvironmentVariableIsSet("GOOFYDECK_DEBUG")) {
        m_enableDebugLogging = true;
    }
    
    if (qEnvironmentVariableIsSet("GOOFYDECK_SOCKET_TIMEOUT")) {
        bool ok;
        int timeout = qEnvironmentVariableIntValue("GOOFYDECK_SOCKET_TIMEOUT", &ok);
        if (ok) {
            m_socketTimeoutMs = timeout;
        }
    }
    
    if (qEnvironmentVariableIsSet("GOOFYDECK_POLLING_INTERVAL")) {
        bool ok;
        int interval = qEnvironmentVariableIntValue("GOOFYDECK_POLLING_INTERVAL", &ok);
        if (ok) {
            m_pollingIntervalMs = interval;
        }
    }
}

void Config::loadFromSettings()
{
    m_settings.beginGroup("Paths");
    m_configPath = m_settings.value("configPath", m_configPath).toString();
    m_pagingSocketPath = m_settings.value("pagingSocketPath", m_pagingSocketPath).toString();
    m_haSocketPath = m_settings.value("haSocketPath", m_haSocketPath).toString();
    m_cacheRoot = m_settings.value("cacheRoot", m_cacheRoot).toString();
    m_settings.endGroup();
    
    m_settings.beginGroup("Timing");
    m_socketTimeoutMs = m_settings.value("socketTimeoutMs", m_socketTimeoutMs).toInt();
    m_pollingIntervalMs = m_settings.value("pollingIntervalMs", m_pollingIntervalMs).toInt();
    m_settings.endGroup();
    
    m_settings.beginGroup("UI");
    m_useFixedSize = m_settings.value("useFixedSize", m_useFixedSize).toBool();
    m_settings.endGroup();
    
    m_settings.beginGroup("Logging");
    m_enableDebugLogging = m_settings.value("enableDebugLogging", m_enableDebugLogging).toBool();
    m_settings.endGroup();
}

void Config::saveToSettings()
{
    m_settings.beginGroup("Paths");
    m_settings.setValue("configPath", m_configPath);
    m_settings.setValue("pagingSocketPath", m_pagingSocketPath);
    m_settings.setValue("haSocketPath", m_haSocketPath);
    m_settings.setValue("cacheRoot", m_cacheRoot);
    m_settings.endGroup();
    
    m_settings.beginGroup("Timing");
    m_settings.setValue("socketTimeoutMs", m_socketTimeoutMs);
    m_settings.setValue("pollingIntervalMs", m_pollingIntervalMs);
    m_settings.endGroup();
    
    m_settings.beginGroup("UI");
    m_settings.setValue("useFixedSize", m_useFixedSize);
    m_settings.endGroup();
    
    m_settings.beginGroup("Logging");
    m_settings.setValue("enableDebugLogging", m_enableDebugLogging);
    m_settings.endGroup();
    
    m_settings.sync();
}
