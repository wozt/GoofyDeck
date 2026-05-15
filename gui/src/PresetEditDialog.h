#ifndef PRESETEDITDIALOG_H
#define PRESETEDITDIALOG_H

#include <QDialog>
#include <QString>
#include <QMap>
#include <QVariant>

class QLineEdit;
class QComboBox;
class QPushButton;
class QTabWidget;
class QTextEdit;

class PresetEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PresetEditDialog(QWidget *parent = nullptr);
    
    // Load presets from full YAML content
    void loadFromYaml(const QString &fullYaml);
    
    // Get modified full YAML content
    QString getModifiedYaml() const;

private slots:
    void onPresetSelected(int index);
    void onNewPreset();
    void onDeletePreset();
    void onSavePreset();

private:
    void setupUI();
    void populatePresetCombo();
    void loadPresetToUI(const QString &presetName);
    void clearUI();
    void updateYamlPreview();
    
    // Parse presets section from YAML
    void parsePresetsSection(const QString &yaml);
    
    // Generate presets section for YAML
    QString generatePresetsSection() const;
    
    // Full YAML content (everything before and after presets)
    QString m_beforePresets;
    QString m_afterPresets;
    
    // Preset data: name -> property map
    QMap<QString, QMap<QString, QString>> m_presets;
    QString m_currentPresetName;
    
    // UI elements
    QComboBox *m_presetCombo = nullptr;
    QTextEdit *m_yamlPreview = nullptr;
    QPushButton *m_newBtn = nullptr;
    QPushButton *m_deleteBtn = nullptr;
    QPushButton *m_saveBtn = nullptr;
    
    // Preset name field
    QLineEdit *m_nameEdit = nullptr;
    
    // Icon settings
    QLineEdit *m_iconBgColorEdit = nullptr;
    QLineEdit *m_iconBorderColorEdit = nullptr;
    QLineEdit *m_iconBorderWidthEdit = nullptr;
    QLineEdit *m_iconBorderRadiusEdit = nullptr;
    QLineEdit *m_iconBorderSizeEdit = nullptr;
    QLineEdit *m_iconSizeEdit = nullptr;
    QLineEdit *m_iconColorEdit = nullptr;
    QLineEdit *m_iconBrightnessEdit = nullptr;
    QLineEdit *m_iconOffsetEdit = nullptr;
    QLineEdit *m_iconPaddingEdit = nullptr;
    
    // Text settings
    QLineEdit *m_textColorEdit = nullptr;
    QLineEdit *m_textSizeEdit = nullptr;
    QLineEdit *m_textFontEdit = nullptr;
    QLineEdit *m_textAlignEdit = nullptr;
    QLineEdit *m_textOffsetEdit = nullptr;
};

#endif // PRESETEDITDIALOG_H
