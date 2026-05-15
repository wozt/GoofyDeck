#ifndef ICONCACHE_H
#define ICONCACHE_H

#include <QObject>
#include <QString>
#include <QPixmap>
#include <QDir>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QMap>

class IconCache : public QObject
{
    Q_OBJECT

public:
    explicit IconCache(QObject *parent = nullptr);
    ~IconCache();
    
    // Find the daemon's session directory in /dev/shm
    bool findSessionCache();
    
    // Start watching for page changes
    void startPageWatcher();
    void stopPageWatcher();
    
    // Get current page from last_page file
    QString getCurrentPage() const;
    int getCurrentOffset() const;
    
    // Get icon for a button (composed wallpaper+icon from wp_comp)
    QPixmap getButtonIcon(int buttonIndex, const QString &pageName = QString());
    
    // Get wallpaper tile for a button position
    QPixmap getWallpaperTile(int position, const QString &pageName = QString());
    
    // Get composed icon (icon + wallpaper overlay) - base method
    QPixmap getComposedIcon(int buttonIndex, const QString &iconName, const QString &pageName);
    
    // Get composed icon with state override
    QPixmap getComposedIconForState(int buttonIndex, const QString &iconName, 
                                     const QString &pageName, const QString &stateValue);
    
    // Get icon with text overlay (for sensors and $cmd.exec_text)
    QPixmap getIconWithText(int buttonIndex, const QString &iconName,
                            const QString &pageName, const QString &text);
    
    // Get icon from current/ directory (for sensors with pre-rendered text)
    QPixmap getIconFromCurrent(int buttonIndex, const QString &pageName);
    
    // Generate icon with text using external draw_text/draw_optimize binaries (Option B)
    QPixmap generateIconWithTextExternal(const QString &baseIconPath, const QString &text,
                                         const QString &textColor = "FFFFFF",
                                         const QString &textAlign = "center",
                                         int textSize = 40,
                                         int textOffsetX = 0, int textOffsetY = 0);
    
    // Generate icon with text from QPixmap (saves to temp file then calls external binaries)
    QPixmap generateIconWithTextExternal(QPixmap basePixmap, const QString &text,
                                         const QString &textColor = "FFFFFF",
                                         const QString &textAlign = "center",
                                         int textSize = 40,
                                         int textOffsetX = 0, int textOffsetY = 0);
    
    // Find wp_comp directory for a specific page
    QString findWpCompDirForPage(const QString &pageName) const;
    
    // Find icon for a specific state value (uses state-specific composition)
    QString findStateIconPath(int buttonIndex, const QString &pageName, 
                              const QString &stateValue, const QString &baseIcon);
    
    // Check if a state-composed icon exists
    bool hasStateIcon(int buttonIndex, const QString &pageName, const QString &stateValue) const;
    
    // Set cache root directory
    void setCacheRoot(const QString &path) { m_cacheRoot = path; }
    void setProjectRoot(const QString &path) { m_projectRoot = path; }
    
    // Check if session cache is available
    bool hasSessionCache() const { return !m_sessionDir.isEmpty(); }
    
signals:
    // Emitted when the current page changes (detected via file watcher)
    void currentPageChanged(const QString &pageName, int offset);
    
    // Emitted when an icon in current/ directory is updated (detected via .last_update file)
    void iconUpdated(int buttonIndex, const QString &pageName);

private slots:
    void onPageFileChanged();
    void checkPageChange();
    void onCurrentDirChanged();

private:
    QString m_sessionDir;      // /dev/shm/goofydeck/private:
    struct WallpaperSettings {
        QString path;
        int quality = 30;
        int magnify = 100;
        bool dithering = true;
    };
    
    QString m_cacheRoot;       // .cache/
    QString m_projectRoot;     // GoofyDeck root
    QString m_lastPage;
    int m_lastOffset;
    QFileSystemWatcher *m_watcher;
    QFileSystemWatcher *m_currentDirWatcher;
    QTimer *m_pollTimer;
    
    QString findConfigFile() const;
    WallpaperSettings loadPageWallpaper(const QString &pageName) const;
    QMap<int, QString> loadPageButtonIcons(const QString &pageName) const;
    uint32_t calculatePageWpSig(const QString &pageName) const;
    uint32_t calculateItemFileHash(const QString &pageName, int itemIndex) const;
    QString calculateIconHash(const QString &iconPath) const;
    QPixmap loadFromPath(const QString &path);
    QString findInSessionCache(const QString &pattern);
    QString findInDiskCache(const QString &subPath);
};

#endif // ICONCACHE_H
