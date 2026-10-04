#ifndef YAMLPARSER_H
#define YAMLPARSER_H

#include <QString>
#include <QMap>
#include <QVector>

struct StateConfig {
    QString stateValue;
    QString name;
    QString icon;
    QStringList presets;
    QString text;
};

struct YamlButton {
    QString name;
    QString icon;
    QString tapAction;
    QString tapData;
    QString holdAction;
    QString longholdAction;
    QString releasedAction;
    QString entityId;
    QStringList presets;
    QMap<QString, StateConfig> states;  // state value -> config
    bool hasStateCmd = false;
    QString stateCmd;
    int stateCmdInterval = 1500;
};

struct YamlPage {
    QString name;
    QVector<YamlButton> buttons;
    QString wallpaperPath;
    int wallpaperQuality = 30;
    int wallpaperMagnify = 100;
    bool wallpaperDithering = true;
};

class YamlParser
{
public:
    YamlParser();
    
    bool parse(const QString &filePath);
    bool parseString(const QString &content);
    
    QVector<YamlPage> pages() const { return m_pages; }
    QStringList pageNames() const;
    YamlPage page(const QString &name) const;
    
    // Save modified content back to file
    bool saveToFile(const QString &filePath, const QString &content);
    
    // Generate YAML content for button
    QString generateButtonYaml(const YamlButton &button, int indent = 6) const;
    
    // Update button in YAML content
    QString updateButtonInYaml(const QString &content, const QString &pageName, int buttonIndex, const YamlButton &newButton);
    
    // Delete button from YAML content
    QString deleteButtonFromYaml(const QString &content, const QString &pageName, int buttonIndex);
    
    // Delete page from YAML content
    QString deletePageFromYaml(const QString &content, const QString &pageName);
    
    // Add new page to YAML content
    QString addPageToYaml(const QString &content, const QString &pageName, const QString &wallpaperPath, const QString &layoutType);
    
    // Simple config getters
    int brightness() const { return m_brightness; }
    QStringList presetNames() const { return m_presets.keys(); }
    
private:
    QVector<YamlPage> m_pages;
    QMap<QString, QVariant> m_presets;
    int m_brightness = 100;
    
    void parsePresets(const QString &content);
    void parsePages(const QString &content);
    void parseButton(YamlPage &page, const QString &buttonContent);
    void parseStates(const QString &buttonContent, YamlButton &btn);
};

#endif // YAMLPARSER_H
