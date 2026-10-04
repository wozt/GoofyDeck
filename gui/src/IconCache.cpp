#include "IconCache.h"
#include "Config.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QRegularExpression>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QCryptographicHash>
#include <QProcess>
#include <QDateTime>

// FNV1A32 hash implementation (same as paging.c)
static uint32_t fnv1a32(const QByteArray &data)
{
    uint32_t h = 2166136261u;
    for (char c : data) {
        h ^= static_cast<uint8_t>(c);
        h *= 16777619u;
    }
    return h;
}

IconCache::IconCache(QObject *parent)
    : QObject(parent)
    , m_cacheRoot(Config::instance().cacheRoot())
    , m_projectRoot(Config::instance().assetsPath())
    , m_watcher(nullptr)
    , m_pollTimer(nullptr)
    , m_currentDirWatcher(new QFileSystemWatcher(this))
    , m_lastOffset(0)
{
    connect(m_currentDirWatcher, &QFileSystemWatcher::directoryChanged,
            this, &IconCache::onCurrentDirChanged);
    connect(m_currentDirWatcher, &QFileSystemWatcher::fileChanged,
            this, &IconCache::onCurrentDirChanged);
}

IconCache::~IconCache()
{
    stopPageWatcher();
}

bool IconCache::findSessionCache()
{
    // Use Config to get session directory
    QString configSessionDir = Config::instance().sessionDir();
    
    // Look for /dev/shm/goofydeck directory (new structure)
    // or /dev/shm/goofydeck_* directories (old structure)
    QDir shmDir("/dev/shm");
    if (!shmDir.exists()) {
        qDebug() << "/dev/shm not available";
        return false;
    }
    
    // First try configured session directory
    if (!configSessionDir.isEmpty() && QDir(configSessionDir).exists()) {
        m_sessionDir = configSessionDir;
        qDebug() << "Found session cache (from config):" << m_sessionDir;
        
        // Initialize last known page from file
        m_lastPage = getCurrentPage();
        m_lastOffset = getCurrentOffset();
        qDebug() << "[IconCache] Initial page:" << m_lastPage << "offset:" << m_lastOffset;
        
        return true;
    }
    
    // First try /dev/shm/goofydeck/paging (new structure)
    QString pagingDir = "/dev/shm/goofydeck/paging";
    if (QDir(pagingDir).exists()) {
        m_sessionDir = pagingDir;
        qDebug() << "Found session cache (new structure):" << m_sessionDir;
        
        // Initialize last known page from file
        m_lastPage = getCurrentPage();
        m_lastOffset = getCurrentOffset();
        qDebug() << "[IconCache] Initial page:" << m_lastPage << "offset:" << m_lastOffset;
        
        // Start watching current/ directory for sensor updates
        QString currentDir = m_sessionDir + "/current";
        if (QDir(currentDir).exists()) {
            m_currentDirWatcher->addPath(currentDir);
            qDebug() << "[IconCache] Watching current/ directory:" << currentDir;
        }
        
        return true;
    }
    
    // Fallback to old structure: goofydeck_*
    QStringList entries = shmDir.entryList(QStringList() << "goofydeck_*", QDir::Dirs);
    if (!entries.isEmpty()) {
        entries.sort();
        QString candidate = shmDir.absoluteFilePath(entries.last()) + "/paging";
        if (QDir(candidate).exists()) {
            m_sessionDir = candidate;
            qDebug() << "Found session cache (old structure):" << m_sessionDir;
            
            // Initialize last known page
            m_lastPage = getCurrentPage();
            m_lastOffset = getCurrentOffset();
            qDebug() << "[IconCache] Initial page:" << m_lastPage << "offset:" << m_lastOffset;
            
            // Start watching current/ directory for sensor updates
            QString currentDir = m_sessionDir + "/current";
            if (QDir(currentDir).exists()) {
                m_currentDirWatcher->addPath(currentDir);
                qDebug() << "[IconCache] Watching current/ directory:" << currentDir;
            }
            
            return true;
        }
    }
    
    qDebug() << "No goofydeck session cache found in /dev/shm";
    return false;
}

QPixmap IconCache::getButtonIcon(int buttonIndex, const QString &pageName)
{
    // FIRST: Check if there's a pre-rendered icon with text in current/ directory
    // This is for sensors and $cmd.exec_text that have text overlays
    QPixmap currentIcon = getIconFromCurrent(buttonIndex, pageName);
    if (!currentIcon.isNull()) {
        qDebug() << "[IconCache] Using current/ icon for button" << buttonIndex;
        return currentIcon;
    }
    
    // Load expected button icons for this page from config
    QMap<int, QString> buttonIcons = loadPageButtonIcons(pageName);
    QString expectedIcon = buttonIcons.value(buttonIndex);
    
    // Try composed icons from wp_comp (these have wallpaper + icon overlay)
    QPixmap composed = getComposedIcon(buttonIndex, expectedIcon, pageName);
    if (!composed.isNull()) {
        return composed;
    }
    
    // Fallback to wallpaper tiles only (no icon overlay)
    return getWallpaperTile(buttonIndex, pageName);
}

QPixmap IconCache::getWallpaperTile(int position, const QString &pageName)
{
    if (m_sessionDir.isEmpty()) return QPixmap();
    
    QString wpDir = m_sessionDir + "/wallpaper";
    QDir dir(wpDir);
    if (!dir.exists()) {
        qDebug() << "[IconCache] Wallpaper directory not found:" << wpDir;
        return QPixmap();
    }
    
    // Get wallpaper settings for this page to find the wallpaper basename
    WallpaperSettings wp = loadPageWallpaper(pageName);
    if (wp.path.isEmpty()) {
        qDebug() << "[IconCache] No wallpaper path found for page" << pageName;
        return QPixmap();
    }
    
    // Extract basename from wallpaper path (e.g., "mymedia/wallpapers/neigesombre16bits.png" -> "neigesombre16bits")
    QString basename = QFileInfo(wp.path).baseName();
    if (basename.isEmpty()) {
        qDebug() << "[IconCache] Could not extract basename from" << wp.path;
        return QPixmap();
    }
    
    qDebug() << "[IconCache] Looking for wallpaper tile with basename:" << basename << "position:" << position;
    
    QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (subdirs.isEmpty()) {
        qDebug() << "[IconCache] No wallpaper subdirectories found";
        return QPixmap();
    }
    
    // Try to find a tile file matching the position and wallpaper basename
    QString expectedFile = QString("%1-%2.png").arg(basename).arg(position);
    
    for (const QString &subdir : subdirs) {
        QDir tileDir(wpDir + "/" + subdir);
        QString tilePath = tileDir.absoluteFilePath(expectedFile);
        qDebug() << "[IconCache] Checking:" << tilePath << "exists:" << QFile::exists(tilePath);
        if (QFile::exists(tilePath)) {
            QPixmap pix = loadFromPath(tilePath);
            if (!pix.isNull()) {
                qDebug() << "[IconCache] LOADED wallpaper tile for button" << position << "from" << tilePath;
                return pix;
            }
        }
    }
    
    qDebug() << "[IconCache] No wallpaper tile found for button" << position << "basename:" << basename;
    return QPixmap();
}

QPixmap IconCache::getComposedIcon(int buttonIndex, const QString &iconName, const QString &pageName)
{
    if (m_sessionDir.isEmpty()) return QPixmap();
    
    // Check if this is a navigation button (these are auto-generated, not from config)
    bool isNavButton = false;
    QString navName;
    if (buttonIndex == 11 && pageName != "$root") {
        isNavButton = true;
        navName = "page_back";
    } else if (iconName.contains("page_prev")) {
        isNavButton = true;
        navName = "page_prev";
    } else if (iconName.contains("page_next")) {
        isNavButton = true;
        navName = "page_next";
    }
    
    // If no icon is configured AND it's not a navigation button, don't search in cache
    // This prevents loading stale/wrong icons from other pages that share the same wallpaper
    if (iconName.isEmpty() && !isNavButton) {
        return QPixmap();
    }
    
    // Calculate wp_sig from page's wallpaper settings
    // This must match paging.c calculation exactly
    uint32_t wpSig = calculatePageWpSig(pageName);
    
    QString targetHash;
    if (wpSig != 0) {
        targetHash = QString("%1").arg(wpSig, 8, 16, QChar('0'));
        qDebug() << "[IconCache] Looking for page" << pageName << "with wp_sig hash:" << targetHash;
    }
    
    // Try specific hash first - this is the ONLY hash we should use
    // The wp_sig hash uniquely identifies the wallpaper for this page
    if (!targetHash.isEmpty()) {
        QString wpCompDir = m_sessionDir + "/wp_comp/" + targetHash;
        QString posDir = wpCompDir + "/" + QString("%1").arg(buttonIndex, 2, 10, QChar('0'));
        
        QDir pdir(posDir);
        if (pdir.exists()) {
            QStringList files = pdir.entryList(QStringList("*.png"), QDir::Files);
            
            // Check for navigation buttons first (special format: <pos>_<nav_name>_<wp_sig>_<pos>.png)
            // navName is already computed above
            if (!navName.isEmpty()) {
                QString navPattern = QString("%1_%2_%3_").arg(buttonIndex).arg(navName).arg(targetHash);
                for (const QString &file : files) {
                    if (file.startsWith(navPattern)) {
                        QString path = posDir + "/" + file;
                        QPixmap pix = loadFromPath(path);
                        if (!pix.isNull()) {
                            qDebug() << "[IconCache] LOADED nav icon for button" << buttonIndex << "from" << path;
                            return pix;
                        }
                    }
                }
            }
            
            // Calculate expected icon hash
            // For external icons (url:/local:), hash is calculated from the URL/path itself
            // For mdi: icons, hash is calculated from page+button (item_file_hash)
            QString expectedPattern;
            if (iconName.startsWith("url:") || iconName.startsWith("local:")) {
                // External icon: hash of the spec itself
                uint32_t iconHash = fnv1a32(iconName.toUtf8());
                // External icons use format: <pos>_<hash>.png (no button number)
                expectedPattern = QString("%1").arg(iconHash, 8, 16, QChar('0'));
                qDebug() << "[IconCache] External icon hash for" << iconName << ":" << expectedPattern;
            } else {
                // MDI or other icons: item_file_hash based on page+button
                // Format: <pos>_<btn>-<hash>.png
                uint32_t iconHash = calculateItemFileHash(pageName, buttonIndex - 1);
                expectedPattern = QString("%1-%2").arg(buttonIndex).arg(iconHash, 8, 16, QChar('0'));
            }
            
            qDebug() << "[IconCache] Looking for icon with pattern:" << expectedPattern << "in" << files;
            
            for (const QString &file : files) {
                // File format: <pos>_<btn>-<file_h>.png
                if (file.contains(expectedPattern)) {
                    QString path = posDir + "/" + file;
                    QPixmap pix = loadFromPath(path);
                    if (!pix.isNull()) {
                        qDebug() << "[IconCache] LOADED icon for button" << buttonIndex << "from" << path;
                        return pix;
                    }
                }
            }
            
            // If no match found, log error and return empty (don't pick wrong icon)
            qDebug() << "[IconCache] No matching icon found for button" << buttonIndex << "expected pattern:" << expectedPattern;
        } else {
            qDebug() << "[IconCache] Directory doesn't exist for button" << buttonIndex << ":" << posDir;
        }
    }
    
    // No fallback to other hashes - they belong to different pages
    // Return empty pixmap so caller can use wallpaper tile instead
    return QPixmap();
}

// Calculate item_file_hash exactly like paging.c does
// Based on page name + button number (stable, no meta)
uint32_t IconCache::calculateItemFileHash(const QString &pageName, int itemIndex) const
{
    // Same logic as paging.c: hash based on "page:<page>\nbutton:<btn>\n"
    int btn = itemIndex + 1;
    QString key = QString("page:%1\nbutton:%2\n").arg(pageName).arg(btn);
    return fnv1a32(key.toUtf8());
}

// Calculate wp_sig hash exactly like paging.c does
// wkey format: "path:%s\nq:%d\nm:%d\nd:%d\n"
uint32_t IconCache::calculatePageWpSig(const QString &pageName) const
{
    // Load wallpaper settings from config file
    WallpaperSettings wp = loadPageWallpaper(pageName);
    
    if (wp.path.isEmpty()) {
        return 0;
    }
    
    QString wkey = QString("path:%1\nq:%2\nm:%3\nd:%4\n")
        .arg(wp.path)
        .arg(wp.quality)
        .arg(wp.magnify)
        .arg(wp.dithering ? 1 : 0);
    
    uint32_t hash = fnv1a32(wkey.toUtf8());
    qDebug() << "[IconCache] Calculated wp_sig for page" << pageName << ":" << QString("%1").arg(hash, 8, 16, QChar('0')) << "wkey:" << wkey;
    
    return hash;
}

// Calculate icon hash (same as daemon's item_file_hash logic)
QString IconCache::calculateIconHash(const QString &iconPath) const
{
    if (iconPath.isEmpty()) return QString();
    
    // The daemon uses fnv1a32 of the icon path/content
    // For simplicity, we use the basename and apply fnv1a32
    QString base = QFileInfo(iconPath).baseName();
    if (base.isEmpty()) return QString();
    
    uint32_t h = fnv1a32(base.toUtf8());
    return QString("%1").arg(h, 8, 16, QChar('0')).toLower();
}

IconCache::WallpaperSettings IconCache::loadPageWallpaper(const QString &pageName) const
{
    WallpaperSettings wp;
    wp.quality = 30;  // defaults from paging.c
    wp.magnify = 100;
    wp.dithering = true;
    
    QString configPath = findConfigFile();
    if (configPath.isEmpty()) {
        return wp;
    }
    
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return wp;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QString content = QString::fromUtf8(data);
    
    // Find the start of pages section
    int pagesSectionPos = content.indexOf("pages:");
    
    // Look for page section
    QString searchPage = pageName + ":";
    int pagePos = content.indexOf(searchPage, pagesSectionPos);
    if (pagePos == -1 && pageName.startsWith("$")) {
        searchPage = pageName;
        pagePos = content.indexOf(searchPage, pagesSectionPos);
    }
    
    if (pagePos != -1) {
        // Find the end of this page section (next page or end of file)
        int nextPagePos = content.indexOf(QRegularExpression("\\n  [a-zA-Z_$][a-zA-Z0-9_]*:"), pagePos + 1);
        int pageEndPos = (nextPagePos != -1) ? nextPagePos : content.length();
        
        // Search for wallpaper only within this page's section
        int wpPos = content.indexOf("wallpaper:", pagePos);
        if (wpPos != -1 && wpPos < pageEndPos) {
            // Extract wallpaper settings from this page's section only
            int pathPos = content.indexOf("path:", wpPos);
            if (pathPos != -1 && pathPos < pageEndPos) {
                int pathValStart = content.indexOf("\"", pathPos);
                int pathValEnd = content.indexOf("\"", pathValStart + 1);
                if (pathValStart != -1 && pathValEnd != -1 && pathValEnd < pageEndPos) {
                    wp.path = content.mid(pathValStart + 1, pathValEnd - pathValStart - 1);
                }
            }
            
            int qualPos = content.indexOf("quality:", wpPos);
            if (qualPos != -1 && qualPos < pageEndPos) {
                int qualValStart = content.indexOf(':', qualPos) + 1;
                while (qualValStart < content.length() && content[qualValStart].isSpace()) {
                    qualValStart++;
                }
                int qualValEnd = qualValStart;
                while (qualValEnd < content.length() && content[qualValEnd].isDigit()) {
                    qualValEnd++;
                }
                QString qualStr = content.mid(qualValStart, qualValEnd - qualValStart).trimmed();
                wp.quality = qualStr.toInt();
            }
            
            int magPos = content.indexOf("magnify:", wpPos);
            if (magPos != -1 && magPos < pageEndPos) {
                int magValStart = content.indexOf(':', magPos) + 1;
                while (magValStart < content.length() && content[magValStart].isSpace()) {
                    magValStart++;
                }
                int magValEnd = magValStart;
                while (magValEnd < content.length() && content[magValEnd].isDigit()) {
                    magValEnd++;
                }
                QString magStr = content.mid(magValStart, magValEnd - magValStart).trimmed();
                wp.magnify = magStr.toInt();
            }
            
            int dithPos = content.indexOf("dithering:", wpPos);
            if (dithPos != -1 && dithPos < pageEndPos) {
                int dithValStart = content.indexOf(':', dithPos) + 1;
                while (dithValStart < content.length() && content[dithValStart].isSpace()) {
                    dithValStart++;
                }
                int dithValEnd = dithValStart;
                while (dithValEnd < content.length() && !content[dithValEnd].isSpace() && content[dithValEnd] != '\n') {
                    dithValEnd++;
                }
                QString dithStr = content.mid(dithValStart, dithValEnd - dithValStart).trimmed().toLower();
                wp.dithering = (dithStr == "yes" || dithStr == "true" || dithStr == "1" || dithStr == "on");
            }
        }
    }
    
    // If no page-specific wallpaper, use global wallpaper (before pages section)
    if (wp.path.isEmpty() && pagesSectionPos != -1) {
        int globalWpPos = content.indexOf("wallpaper:", 0);
        // Make sure we get the global one (before pages section)
        if (globalWpPos != -1 && globalWpPos < pagesSectionPos) {
            int pathPos = content.indexOf("path:", globalWpPos);
            if (pathPos != -1 && pathPos < pagesSectionPos) {
                int pathValStart = content.indexOf("\"", pathPos);
                int pathValEnd = content.indexOf("\"", pathValStart + 1);
                if (pathValStart != -1 && pathValEnd != -1 && pathValEnd < pagesSectionPos) {
                    wp.path = content.mid(pathValStart + 1, pathValEnd - pathValStart - 1);
                }
            }
            
            // Also extract quality, magnify, dithering from global wallpaper
            int qualPos = content.indexOf("quality:", globalWpPos);
            if (qualPos != -1 && qualPos < pagesSectionPos) {
                int qualValStart = content.indexOf(':', qualPos) + 1;
                while (qualValStart < content.length() && content[qualValStart].isSpace()) qualValStart++;
                int qualValEnd = qualValStart;
                while (qualValEnd < content.length() && content[qualValEnd].isDigit()) qualValEnd++;
                wp.quality = content.mid(qualValStart, qualValEnd - qualValStart).trimmed().toInt();
            }
            
            int magPos = content.indexOf("magnify:", globalWpPos);
            if (magPos != -1 && magPos < pagesSectionPos) {
                int magValStart = content.indexOf(':', magPos) + 1;
                while (magValStart < content.length() && content[magValStart].isSpace()) magValStart++;
                int magValEnd = magValStart;
                while (magValEnd < content.length() && content[magValEnd].isDigit()) magValEnd++;
                wp.magnify = content.mid(magValStart, magValEnd - magValStart).trimmed().toInt();
            }
            
            int dithPos = content.indexOf("dithering:", globalWpPos);
            if (dithPos != -1 && dithPos < pagesSectionPos) {
                int dithValStart = content.indexOf(':', dithPos) + 1;
                while (dithValStart < content.length() && content[dithValStart].isSpace()) dithValStart++;
                int dithValEnd = dithValStart;
                while (dithValEnd < content.length() && !content[dithValEnd].isSpace() && content[dithValEnd] != '\n') dithValEnd++;
                QString dithStr = content.mid(dithValStart, dithValEnd - dithValStart).trimmed().toLower();
                wp.dithering = (dithStr == "yes" || dithStr == "true" || dithStr == "1" || dithStr == "on");
            }
        }
    }
    
    return wp;
}

// Load button icons for a page from configuration
QMap<int, QString> IconCache::loadPageButtonIcons(const QString &pageName) const
{
    QMap<int, QString> icons;
    
    QString configPath = findConfigFile();
    if (configPath.isEmpty()) {
        return icons;
    }
    
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return icons;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QString content = QString::fromUtf8(data);
    
    // Find the start of pages section
    int pagesSectionPos = content.indexOf("pages:");
    
    // Look for page section
    QString searchPage = pageName + ":";
    int pagePos = content.indexOf(searchPage, pagesSectionPos);
    if (pagePos == -1 && pageName.startsWith("$")) {
        searchPage = pageName;
        pagePos = content.indexOf(searchPage, pagesSectionPos);
    }
    
    if (pagePos == -1) {
        return icons;
    }
    
    // Find the end of this page section (next page or end of file)
    int nextPagePos = content.indexOf(QRegularExpression("\\n  [a-zA-Z_$][a-zA-Z0-9_]*:"), pagePos + 1);
    int pageEndPos = (nextPagePos != -1) ? nextPagePos : content.length();
    
    // Find buttons section
    int buttonsPos = content.indexOf("buttons:", pagePos);
    if (buttonsPos == -1 || buttonsPos >= pageEndPos) {
        return icons;
    }
    
    // Parse buttons - each button starts with "- name:"
    int buttonPos = content.indexOf("- name:", buttonsPos);
    int buttonIndex = 1;
    
    while (buttonPos != -1 && buttonPos < pageEndPos && buttonIndex <= 14) {
        // Find icon for this button
        // Look for "icon:" within this button's definition
        int nextButtonPos = content.indexOf("- name:", buttonPos + 1);
        if (nextButtonPos == -1 || nextButtonPos > pageEndPos) {
            nextButtonPos = pageEndPos;
        }
        
        QString iconName;
        
        int iconPos = content.indexOf("icon:", buttonPos);
        if (iconPos != -1 && iconPos < nextButtonPos) {
            // Extract icon value
            int iconValStart = content.indexOf("mdi:", iconPos);
            if (iconValStart != -1 && iconValStart < nextButtonPos) {
                int iconValEnd = iconValStart;
                while (iconValEnd < content.length() && !content[iconValEnd].isSpace() && content[iconValEnd] != '\n') {
                    iconValEnd++;
                }
                iconName = content.mid(iconValStart, iconValEnd - iconValStart).trimmed();
            } else {
                // Check for url: icons - keep the full "url:" prefix
                iconValStart = content.indexOf("url:", iconPos);
                if (iconValStart != -1 && iconValStart < nextButtonPos) {
                    int iconValEnd = iconValStart;
                    while (iconValEnd < content.length() && content[iconValEnd] != '\n') {
                        iconValEnd++;
                    }
                    // Keep the full "url:..." spec for hash calculation
                    iconName = content.mid(iconValStart, iconValEnd - iconValStart).trimmed();
                }
            }
        }
        
        // If no icon found, try to find it in states section or check presets
        if (iconName.isEmpty()) {
            // First check if button has states with icon
            int statesPos = content.indexOf("states:", buttonPos);
            if (statesPos != -1 && statesPos < nextButtonPos) {
                // Look for icon in the "on" state
                int onPos = content.indexOf("\"on\":", statesPos);
                if (onPos == -1) {
                    onPos = content.indexOf("on:", statesPos);
                }
                if (onPos != -1 && onPos < nextButtonPos) {
                    int onIconPos = content.indexOf("icon:", onPos);
                    if (onIconPos != -1 && onIconPos < nextButtonPos) {
                        int iconValStart = content.indexOf("mdi:", onIconPos);
                        if (iconValStart != -1 && iconValStart < nextButtonPos) {
                            int iconValEnd = iconValStart;
                            while (iconValEnd < content.length() && !content[iconValEnd].isSpace() && content[iconValEnd] != '\n') {
                                iconValEnd++;
                            }
                            iconName = content.mid(iconValStart, iconValEnd - iconValStart).trimmed();
                        }
                    }
                    // Also check for presets in states that might indicate light
                    if (iconName.isEmpty()) {
                        int presetPos = content.indexOf("presets:", onPos);
                        if (presetPos != -1 && presetPos < nextButtonPos) {
                            int presetValStart = content.indexOf("light_on", presetPos);
                            if (presetValStart != -1 && presetValStart < nextButtonPos) {
                                iconName = "mdi:lightbulb";
                            }
                        }
                    }
                }
            }
            
            // Also check main button presets for light indicators
            if (iconName.isEmpty()) {
                int presetPos = content.indexOf("presets:", buttonPos);
                if (presetPos != -1 && presetPos < nextButtonPos) {
                    // Check if any preset contains "light" indicator
                    int presetEnd = nextButtonPos;
                    QString presetSection = content.mid(presetPos, presetEnd - presetPos);
                    if (presetSection.contains("light_on") || presetSection.contains("light_off")) {
                        iconName = "mdi:lightbulb";
                    }
                }
            }
        }
        
        // If still no icon but has entity_id starting with light. or switch., use default
        if (iconName.isEmpty()) {
            int entityPos = content.indexOf("entity_id:", buttonPos);
            if (entityPos != -1 && entityPos < nextButtonPos) {
                int entityValStart = content.indexOf(':', entityPos) + 1;
                while (entityValStart < content.length() && content[entityValStart].isSpace()) entityValStart++;
                int entityValEnd = entityValStart;
                while (entityValEnd < content.length() && !content[entityValEnd].isSpace() && content[entityValEnd] != '\n') {
                    entityValEnd++;
                }
                QString entityId = content.mid(entityValStart, entityValEnd - entityValStart).trimmed();
                if (entityId.startsWith("light.")) {
                    iconName = "mdi:lightbulb";
                } else if (entityId.startsWith("switch.")) {
                    iconName = "mdi:toggle-switch";
                }
            }
        }
        
        if (!iconName.isEmpty()) {
            icons[buttonIndex] = iconName;
        }
        
        buttonPos = nextButtonPos;
        buttonIndex++;
    }
    
    return icons;
}

QString IconCache::findConfigFile() const
{
    QStringList possiblePaths;
    possiblePaths << "../config/configuration.yml"
                  << "../../config/configuration.yml"
                  << "/home/wozt/GoofyDeck/config/configuration.yml"
                  << "config/configuration.yml";
    
    for (const QString &path : possiblePaths) {
        if (QFile::exists(path)) {
            return path;
        }
    }
    
    return QString();
}

QPixmap IconCache::loadFromPath(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        return QPixmap();
    }
    
    QPixmap pix(path);
    if (pix.isNull()) {
        qDebug() << "Failed to load image from:" << path;
    }
    return pix;
}

QString IconCache::findInSessionCache(const QString &pattern)
{
    if (m_sessionDir.isEmpty()) return QString();
    
    QDir dir(m_sessionDir);
    QStringList files = dir.entryList(QStringList(pattern), QDir::Files);
    if (!files.isEmpty()) {
        return m_sessionDir + "/" + files.first();
    }
    return QString();
}

QString IconCache::findInDiskCache(const QString &subPath)
{
    QString fullPath = m_cacheRoot + "/" + subPath;
    QFileInfo info(fullPath);
    if (info.exists() && info.isFile()) {
        return fullPath;
    }
    return QString();
}

void IconCache::startPageWatcher()
{
    if (m_sessionDir.isEmpty()) return;
    
    QString lastPageFile = m_sessionDir + "/last_page";
    QString lastOffsetFile = m_sessionDir + "/last_offset";
    QString currentDir = m_sessionDir + "/current";
    QString lastUpdateFile = currentDir + "/.last_update";
    
    // Create watcher if not exists
    if (!m_watcher) {
        m_watcher = new QFileSystemWatcher(this);
        connect(m_watcher, &QFileSystemWatcher::fileChanged, 
                this, &IconCache::onPageFileChanged);
        connect(m_watcher, &QFileSystemWatcher::directoryChanged,
                this, &IconCache::onPageFileChanged);
    }
    
    // Watch both files AND the directory (more reliable for files that get recreated)
    if (QFile::exists(lastPageFile)) {
        m_watcher->addPath(lastPageFile);
        qDebug() << "[IconCache] Watching file:" << lastPageFile;
    }
    if (QFile::exists(lastOffsetFile)) {
        m_watcher->addPath(lastOffsetFile);
        qDebug() << "[IconCache] Watching file:" << lastOffsetFile;
    }
    // Also watch the directory itself for any changes
    m_watcher->addPath(m_sessionDir);
    qDebug() << "[IconCache] Watching directory:" << m_sessionDir;
    
    // Watch current/ directory for icon updates
    if (!m_currentDirWatcher) {
        m_currentDirWatcher = new QFileSystemWatcher(this);
        connect(m_currentDirWatcher, &QFileSystemWatcher::fileChanged,
                this, &IconCache::onCurrentDirChanged);
    }
    if (QFile::exists(currentDir)) {
        m_currentDirWatcher->addPath(currentDir);
        qDebug() << "[IconCache] Watching current/ directory:" << currentDir;
    }
    if (QFile::exists(lastUpdateFile)) {
        m_currentDirWatcher->addPath(lastUpdateFile);
        qDebug() << "[IconCache] Watching .last_update file:" << lastUpdateFile;
    }
    
    // Also use polling as backup - more frequent for better responsiveness
    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        connect(m_pollTimer, &QTimer::timeout, this, &IconCache::checkPageChange);
        m_pollTimer->start(500); // Check every 500ms
        qDebug() << "[IconCache] STARTED polling timer (500ms) - timer active:" << m_pollTimer->isActive();
    }
}

void IconCache::stopPageWatcher()
{
    if (m_watcher) {
        m_watcher->deleteLater();
        m_watcher = nullptr;
    }
    if (m_currentDirWatcher) {
        m_currentDirWatcher->deleteLater();
        m_currentDirWatcher = nullptr;
    }
    if (m_pollTimer) {
        m_pollTimer->stop();
        m_pollTimer->deleteLater();
        m_pollTimer = nullptr;
    }
}

QString IconCache::getCurrentPage() const
{
    if (m_sessionDir.isEmpty()) return QString();
    
    QString lastPageFile = m_sessionDir + "/last_page";
    QFile file(lastPageFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QString page = QString::fromUtf8(data).trimmed();
    page = page.split('\n').first().trimmed();
    return page;
}

int IconCache::getCurrentOffset() const
{
    if (m_sessionDir.isEmpty()) return 0;
    
    QString lastOffsetFile = m_sessionDir + "/last_offset";
    QFile file(lastOffsetFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    bool ok;
    int offset = QString::fromUtf8(data).trimmed().toInt(&ok);
    return ok ? offset : 0;
}

void IconCache::onPageFileChanged()
{
    checkPageChange();
}

void IconCache::onCurrentDirChanged()
{
    // Read .last_update file to get which button was updated
    QString currentDir = m_sessionDir + "/current";
    QString lastUpdateFile = currentDir + "/.last_update";
    
    QFile file(lastUpdateFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // If we can't read the file, emit a generic update for all buttons
        qDebug() << "[IconCache] Current/ directory changed, refreshing all buttons";
        for (int i = 1; i <= 13; i++) {
            emit iconUpdated(i, m_lastPage);
        }
        return;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QString content = QString::fromUtf8(data).trimmed();
    // Parse format: "<page>_<pos>.png" e.g., "salon_01.png"
    QRegularExpression re("^(.*)_(\\d+)\\.png$");
    QRegularExpressionMatch match = re.match(content);
    
    if (match.hasMatch()) {
        QString pageName = match.captured(1);
        int buttonIndex = match.captured(2).toInt();
        qDebug() << "[IconCache] Icon updated for button" << buttonIndex << "page" << pageName;
        emit iconUpdated(buttonIndex, pageName);
    } else {
        // Fallback: refresh all buttons
        qDebug() << "[IconCache] Could not parse .last_update content:" << content;
        for (int i = 1; i <= 13; i++) {
            emit iconUpdated(i, m_lastPage);
        }
    }
}

void IconCache::checkPageChange()
{
    QString currentPage = getCurrentPage();
    int currentOffset = getCurrentOffset();
    
    if (currentPage.isEmpty()) {
        return;
    }
    
    if (currentPage != m_lastPage || currentOffset != m_lastOffset) {
        qDebug() << "[IconCache] PAGE CHANGED:" << m_lastPage << "->" << currentPage;
        m_lastPage = currentPage;
        m_lastOffset = currentOffset;
        emit currentPageChanged(currentPage, currentOffset);
    }
}

QString IconCache::findWpCompDirForPage(const QString &pageName) const
{
    // This function is no longer used - we now use timestamps to find the most recent icons
    // across all hash directories
    (void)pageName;
    return QString();
}

// Get composed icon for a specific state value
QPixmap IconCache::getComposedIconForState(int buttonIndex, const QString &iconName,
                                            const QString &pageName, const QString &stateValue)
{
    if (m_sessionDir.isEmpty()) return QPixmap();
    if (stateValue.isEmpty()) return QPixmap();
    
    qDebug() << "[IconCache] getComposedIconForState: button" << buttonIndex 
             << "page" << pageName << "state" << stateValue << "icon" << iconName;
    
    // Try wp_comp directory first (composed icons with wallpaper)
    QString stateIconPath = findStateIconPath(buttonIndex, pageName, stateValue, iconName);
    if (!stateIconPath.isEmpty()) {
        QPixmap pix = loadFromPath(stateIconPath);
        if (!pix.isNull()) {
            qDebug() << "[IconCache] LOADED state icon from wp_comp:" << stateIconPath;
            return pix;
        }
    }
    
    // Fallback: try .cache directory (where paging.c stores state icons)
    QString cachePageDir = m_cacheRoot + "/" + pageName;
    QDir cacheDir(cachePageDir);
    
    if (cacheDir.exists()) {
        QStringList filters;
        filters << QString("%1-*.png").arg(buttonIndex);
        cacheDir.setNameFilters(filters);
        QFileInfoList files = cacheDir.entryInfoList(QDir::Files);
        
        // Look for state-specific file
        QString statePattern = "-" + stateValue + ".png";
        for (const QFileInfo &fileInfo : files) {
            QString fileName = fileInfo.fileName();
            if (fileName.endsWith(statePattern, Qt::CaseInsensitive)) {
                QString fullPath = fileInfo.absoluteFilePath();
                QPixmap pix = loadFromPath(fullPath);
                if (!pix.isNull()) {
                    qDebug() << "[IconCache] LOADED state icon from .cache:" << fullPath;
                    return pix;
                }
            }
        }
    }
    
    // Final fallback to regular composed icon
    qDebug() << "[IconCache] Fallback to regular composed icon for button" << buttonIndex;
    return getComposedIcon(buttonIndex, iconName, pageName);
}

// Get icon with text overlay (for sensors and $cmd.exec_text)
QPixmap IconCache::getIconWithText(int buttonIndex, const QString &iconName,
                                    const QString &pageName, const QString &text)
{
    qDebug() << "[IconCache] getIconWithText called for button" << buttonIndex 
             << "page" << pageName << "text" << text;
    
    if (m_sessionDir.isEmpty()) {
        qDebug() << "[IconCache] sessionDir is empty!";
        return QPixmap();
    }
    if (text.isEmpty()) {
        qDebug() << "[IconCache] text is empty, returning regular icon";
        // No text, just return regular icon
        return getComposedIcon(buttonIndex, iconName, pageName);
    }
    
    // The daemon saves current icons with text to current/ directory
    // Format: <page_sanitized>_<pos>.png (e.g., lumiere_06.png)
    QString currentDir = m_sessionDir + "/current";
    
    // Sanitize page name like paging.c does
    QString pageTag = pageName;
    if (pageTag.startsWith("$")) pageTag = pageTag.mid(1);
    pageTag.replace(QRegularExpression("[^a-zA-Z0-9_]"), "_");
    
    QString fileName = QString("%1_%2.png").arg(pageTag).arg(buttonIndex, 2, 10, QChar('0'));
    QString filePath = currentDir + "/" + fileName;
    
    qDebug() << "[IconCache] Looking for text icon at:" << filePath 
             << "exists:" << QFile::exists(filePath);
    
    if (QFile::exists(filePath)) {
        QPixmap pix = loadFromPath(filePath);
        if (!pix.isNull()) {
            qDebug() << "[IconCache] LOADED text icon from current/ for button" << buttonIndex 
                     << ":" << filePath;
            return pix;
        } else {
            qDebug() << "[IconCache] FAILED to load pixmap from:" << filePath;
        }
    }
    
    qDebug() << "[IconCache] Falling back to regular icon for button" << buttonIndex;
    // Fallback to base icon
    return getComposedIcon(buttonIndex, iconName, pageName);
}

// Get icon from current/ directory (for sensors with pre-rendered text)
QPixmap IconCache::getIconFromCurrent(int buttonIndex, const QString &pageName)
{
    if (m_sessionDir.isEmpty()) {
        return QPixmap();
    }
    
    // The daemon saves current icons with text to current/ directory
    // Format: <page_sanitized>_<pos>.png (e.g., salon_01.png)
    QString currentDir = m_sessionDir + "/current";
    
    // Sanitize page name like paging.c does
    QString pageTag = pageName;
    if (pageTag.startsWith("$")) pageTag = pageTag.mid(1);
    pageTag.replace(QRegularExpression("[^a-zA-Z0-9_]"), "_");
    
    QString fileName = QString("%1_%2.png").arg(pageTag).arg(buttonIndex, 2, 10, QChar('0'));
    QString filePath = currentDir + "/" + fileName;
    
    qDebug() << "[IconCache] Looking for current/ icon at:" << filePath 
             << "exists:" << QFile::exists(filePath);
    
    if (QFile::exists(filePath)) {
        // Read file into memory first to avoid QPixmap cache issues
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            file.close();
            
            QPixmap pix;
            if (pix.loadFromData(data)) {
                qDebug() << "[IconCache] LOADED icon from current/ for button" << buttonIndex 
                         << ":" << filePath << "size:" << data.size();
                return pix;
            } else {
                qDebug() << "[IconCache] FAILED to load pixmap from data for:" << filePath;
            }
        } else {
            qDebug() << "[IconCache] FAILED to open file:" << filePath;
        }
    }
    
    return QPixmap();
}

// Generate icon with text using external draw_text/draw_optimize binaries (Option B)
QPixmap IconCache::generateIconWithTextExternal(const QString &baseIconPath, const QString &text,
                                                   const QString &textColor, const QString &textAlign,
                                                   int textSize, int textOffsetX, int textOffsetY)
{
    if (baseIconPath.isEmpty() || text.isEmpty()) return QPixmap();
    if (!QFile::exists(baseIconPath)) return QPixmap();
    
    // Find project root to locate binaries
    QString projectRoot;
    QStringList possibleRoots;
    possibleRoots << ".." << "../.." << "/home/wozt/GoofyDeck" << ".";
    for (const QString &root : possibleRoots) {
        QString iconsDir = root + "/icons";
        if (QDir(iconsDir).exists()) {
            projectRoot = root;
            break;
        }
    }
    if (projectRoot.isEmpty()) {
        qDebug() << "[IconCache] Could not find project root for draw_text/draw_optimize";
        return QPixmap();
    }
    
    QString drawTextBin = projectRoot + "/icons/draw_text";
    QString drawOptBin = projectRoot + "/icons/draw_optimize";
    
    if (!QFile::exists(drawTextBin)) {
        qDebug() << "[IconCache] draw_text not found at:" << drawTextBin;
        return QPixmap();
    }
    if (!QFile::exists(drawOptBin)) {
        qDebug() << "[IconCache] draw_optimize not found at:" << drawOptBin;
        return QPixmap();
    }
    
    // Create temp file for output
    QString tmpDir = QDir::tempPath();
    QString tmpOut = tmpDir + "/goofydeck_text_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".png";
    
    // Copy base icon to temp file
    if (!QFile::copy(baseIconPath, tmpOut)) {
        qDebug() << "[IconCache] Failed to copy base icon to temp file";
        return QPixmap();
    }
    
    // Prepare arguments for draw_text
    QStringList textArgs;
    textArgs << "--text=" + text;
    textArgs << "--text_color=" + textColor;
    textArgs << "--text_align=" + textAlign;
    textArgs << "--text_size=" + QString::number(textSize);
    textArgs << "--text_offset=" + QString::number(textOffsetX) + "," + QString::number(textOffsetY);
    textArgs << tmpOut;
    
    // Run draw_text
    QProcess textProcess;
    textProcess.start(drawTextBin, textArgs);
    if (!textProcess.waitForFinished(5000)) {
        qDebug() << "[IconCache] draw_text timed out or failed:" << textProcess.errorString();
        QFile::remove(tmpOut);
        return QPixmap();
    }
    if (textProcess.exitCode() != 0) {
        qDebug() << "[IconCache] draw_text failed with exit code:" << textProcess.exitCode();
        qDebug() << "[IconCache] stderr:" << textProcess.readAllStandardError();
        QFile::remove(tmpOut);
        return QPixmap();
    }
    
    // Run draw_optimize
    QStringList optArgs;
    optArgs << "-c" << "4";
    optArgs << tmpOut;
    
    QProcess optProcess;
    optProcess.start(drawOptBin, optArgs);
    if (!optProcess.waitForFinished(5000)) {
        qDebug() << "[IconCache] draw_optimize timed out";
        // Continue anyway, image might still be usable
    }
    
    // Load the result
    QPixmap result(tmpOut);
    if (result.isNull()) {
        qDebug() << "[IconCache] Failed to load generated icon from:" << tmpOut;
    } else {
        qDebug() << "[IconCache] Successfully generated icon with text:" << text;
    }
    
    // Clean up temp file
    QFile::remove(tmpOut);
    
    return result;
}

// Generate icon with text from QPixmap (saves to temp file then calls external binaries)
QPixmap IconCache::generateIconWithTextExternal(QPixmap basePixmap, const QString &text,
                                                 const QString &textColor, const QString &textAlign,
                                                 int textSize, int textOffsetX, int textOffsetY)
{
    if (basePixmap.isNull() || text.isEmpty()) return QPixmap();
    
    // Save base pixmap to temp file
    QString tmpDir = QDir::tempPath();
    QString tmpBase = tmpDir + "/goofydeck_base_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".png";
    if (!basePixmap.save(tmpBase, "PNG")) {
        qDebug() << "[IconCache] Failed to save base pixmap to temp file";
        return QPixmap();
    }
    
    // Call the file-based version
    QPixmap result = generateIconWithTextExternal(tmpBase, text, textColor, textAlign, 
                                                    textSize, textOffsetX, textOffsetY);
    
    // Clean up temp base file
    QFile::remove(tmpBase);
    
    return result;
}

// Find icon path for a specific state value
QString IconCache::findStateIconPath(int buttonIndex, const QString &pageName,
                                     const QString &stateValue, const QString &baseIcon)
{
    if (m_sessionDir.isEmpty()) return QString();
    
    // First check current/ directory (daemon saves current icons there)
    QString currentDir = m_sessionDir + "/current";
    QString pageTag = pageName;
    if (pageTag.startsWith("$")) pageTag = pageTag.mid(1);
    pageTag.replace(QRegularExpression("[^a-zA-Z0-9_]"), "_");
    
    QString fileName = QString("%1_%2.png").arg(pageTag).arg(buttonIndex, 2, 10, QChar('0'));
    QString filePath = currentDir + "/" + fileName;
    
    if (QFile::exists(filePath)) {
        return filePath;
    }
    
    // Fallback: check wp_comp for state-specific icons
    uint32_t wpSig = calculatePageWpSig(pageName);
    if (wpSig == 0) return QString();
    
    QString targetHash = QString("%1").arg(wpSig, 8, 16, QChar('0'));
    QString wpCompDir = m_sessionDir + "/wp_comp/" + targetHash;
    QString posDir = wpCompDir + "/" + QString("%1").arg(buttonIndex, 2, 10, QChar('0'));
    
    QDir pdir(posDir);
    if (!pdir.exists()) return QString();
    
    // State-specific icons have the state value in their filename
    QString statePattern = "_" + stateValue + ".png";
    
    QStringList files = pdir.entryList(QStringList("*.png"), QDir::Files);
    
    // First try to find exact state match
    for (const QString &file : files) {
        if (file.contains(statePattern, Qt::CaseInsensitive)) {
            return posDir + "/" + file;
        }
    }
    
    // Also try generic pattern matching for common states
    QString genericState;
    if (stateValue == "on" || stateValue == "ON" || stateValue == "true") {
        genericState = "_on.png";
    } else if (stateValue == "off" || stateValue == "OFF" || stateValue == "false") {
        genericState = "_off.png";
    }
    
    if (!genericState.isEmpty()) {
        for (const QString &file : files) {
            if (file.endsWith(genericState, Qt::CaseInsensitive)) {
                return posDir + "/" + file;
            }
        }
    }
    
    return QString();
}

// Check if a state-composed icon exists
bool IconCache::hasStateIcon(int buttonIndex, const QString &pageName, const QString &stateValue) const
{
    // Check if we can find a state-specific icon
    QString path = const_cast<IconCache*>(this)->findStateIconPath(buttonIndex, pageName, stateValue, QString());
    return !path.isEmpty() && QFile::exists(path);
}

QPixmap IconCache::regenerateSensorIcon(int buttonIndex, const QString &pageName, 
                                         const QString &text, const QString &baseIcon)
{
    if (m_projectRoot.isEmpty()) {
        qDebug() << "[IconCache] Project root not set, cannot regenerate sensor icon";
        return QPixmap();
    }
    
    qDebug() << "[IconCache] Regenerating sensor icon for button" << buttonIndex 
             << "page" << pageName << "text" << text;
    
    // Find base icon
    QString baseIconPath;
    if (!baseIcon.isEmpty()) {
        // Use provided base icon name - try to find it in assets
        if (baseIcon.startsWith("mdi:")) {
            // MDI icon - need to generate it first
            baseIconPath = m_projectRoot + "/assets/pregen/blank.png";
        } else if (baseIcon.startsWith("/")) {
            // Absolute path
            baseIconPath = baseIcon;
        } else {
            // Relative path
            baseIconPath = m_projectRoot + "/" + baseIcon;
        }
    } else {
        // Try to get base icon from cache
        QPixmap basePixmap = getButtonIcon(buttonIndex, pageName);
        if (basePixmap.isNull()) {
            // Fallback to blank icon
            baseIconPath = m_projectRoot + "/assets/pregen/blank.png";
        } else {
            // Save the pixmap to a temp file to use as base
            QString tmpDir = m_sessionDir + "/tmp";
            QDir tmp(tmpDir);
            if (!tmp.exists()) {
                tmp.mkpath(tmpDir);
            }
            baseIconPath = QString("%1/base_%2_%3.png").arg(tmpDir).arg(buttonIndex).arg(QDateTime::currentMSecsSinceEpoch());
            basePixmap.save(baseIconPath);
        }
    }
    
    if (!QFile::exists(baseIconPath)) {
        qDebug() << "[IconCache] Base icon not found:" << baseIconPath;
        return QPixmap();
    }
    
    // Create temp directory for output
    QString tmpDir = m_sessionDir + "/tmp";
    QDir tmp(tmpDir);
    if (!tmp.exists()) {
        tmp.mkpath(tmpDir);
    }
    
    // Sanitize page name for filename
    QString pageTag = pageName;
    pageTag.replace(QRegularExpression("[^a-zA-Z0-9_]"), "_");
    
    // Generate unique output filename
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    QString outPath = QString("%1/sensor_%2_%3_%4.png").arg(tmpDir).arg(pageTag).arg(buttonIndex).arg(timestamp);
    
    // Copy base icon to output
    if (!QFile::copy(baseIconPath, outPath)) {
        qDebug() << "[IconCache] Failed to copy base icon to:" << outPath;
        return QPixmap();
    }
    
    // Call draw_text to add text overlay
    QString drawTextBin = m_projectRoot + "/icons/draw_text";
    if (!QFile::exists(drawTextBin)) {
        qDebug() << "[IconCache] draw_text binary not found:" << drawTextBin;
        QFile::remove(outPath);
        return QPixmap();
    }
    
    // Default text parameters (from cmd_button preset in config)
    QString textColor = "00FF00";
    QString textAlign = "center";
    QString textFont = "dustismo_bold.ttf";
    int textSize = 50;
    int textOffsetX = 0;
    int textOffsetY = 0;
    
    // Build command
    QStringList args;
    args << QString("--text=%1").arg(text);
    args << QString("--text_color=%1").arg(textColor);
    args << QString("--text_align=%1").arg(textAlign);
    args << QString("--text_font=%1").arg(textFont);
    args << QString("--text_size=%1").arg(textSize);
    args << QString("--text_offset=%1,%2").arg(textOffsetX).arg(textOffsetY);
    args << outPath;
    
    QProcess process;
    process.start(drawTextBin, args);
    if (!process.waitForFinished(5000)) {
        qDebug() << "[IconCache] draw_text timed out";
        QFile::remove(outPath);
        return QPixmap();
    }
    
    if (process.exitCode() != 0) {
        qDebug() << "[IconCache] draw_text failed with exit code:" << process.exitCode();
        qDebug() << "[IconCache] stderr:" << process.readAllStandardError();
        QFile::remove(outPath);
        return QPixmap();
    }
    
    // Call draw_optimize to optimize the image
    QString drawOptBin = m_projectRoot + "/icons/draw_optimize";
    if (QFile::exists(drawOptBin)) {
        QStringList optArgs;
        optArgs << outPath;
        QProcess optProcess;
        optProcess.start(drawOptBin, optArgs);
        if (optProcess.waitForFinished(5000)) {
            qDebug() << "[IconCache] draw_optimize completed";
        }
    }
    
    // Save to current/ directory
    QString currentDir = m_sessionDir + "/current";
    QDir cdir(currentDir);
    if (!cdir.exists()) {
        cdir.mkpath(currentDir);
    }
    
    QString currentPath = QString("%1/%2_%3.png").arg(currentDir).arg(pageTag).arg(buttonIndex, 2, 10, QChar('0'));
    if (QFile::exists(currentPath)) {
        QFile::remove(currentPath);
    }
    QFile::copy(outPath, currentPath);
    
    // Load and return the generated icon
    QPixmap result = loadFromPath(currentPath);
    
    // Clean up temp file
    QFile::remove(outPath);
    
    if (!result.isNull()) {
        qDebug() << "[IconCache] Successfully regenerated sensor icon:" << currentPath;
    }
    
    return result;
}
