#include "PresetEditDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QTextEdit>
#include <QRegularExpression>

PresetEditDialog::PresetEditDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUI();
    setWindowTitle("Edit Presets");
    setModal(true);
    resize(600, 700);
}

void PresetEditDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Preset selector
    QHBoxLayout *selectorLayout = new QHBoxLayout();
    selectorLayout->addWidget(new QLabel("Select Preset:"));
    
    m_presetCombo = new QComboBox(this);
    m_presetCombo->setEditable(false);
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PresetEditDialog::onPresetSelected);
    selectorLayout->addWidget(m_presetCombo, 1);
    
    m_newBtn = new QPushButton("Create New...", this);
    m_newBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");
    connect(m_newBtn, &QPushButton::clicked, this, &PresetEditDialog::onNewPreset);
    selectorLayout->addWidget(m_newBtn);
    
    m_deleteBtn = new QPushButton("Delete", this);
    m_deleteBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; }");
    connect(m_deleteBtn, &QPushButton::clicked, this, &PresetEditDialog::onDeletePreset);
    selectorLayout->addWidget(m_deleteBtn);
    
    mainLayout->addLayout(selectorLayout);
    
    // Preset name field
    QHBoxLayout *nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel("Preset Name:"));
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("Enter preset name (e.g., my_custom_preset)");
    nameLayout->addWidget(m_nameEdit, 1);
    mainLayout->addLayout(nameLayout);
    
    // Tab widget
    QTabWidget *tabWidget = new QTabWidget(this);
    
    // Icon settings tab
    QWidget *iconTab = new QWidget();
    QFormLayout *iconLayout = new QFormLayout(iconTab);
    
    m_iconBgColorEdit = new QLineEdit(this);
    m_iconBgColorEdit->setPlaceholderText("transparent or #RRGGBB");
    iconLayout->addRow("Background Color:", m_iconBgColorEdit);
    
    m_iconBorderColorEdit = new QLineEdit(this);
    m_iconBorderColorEdit->setPlaceholderText("FFFFFF");
    iconLayout->addRow("Border Color:", m_iconBorderColorEdit);
    
    m_iconBorderWidthEdit = new QLineEdit(this);
    m_iconBorderWidthEdit->setPlaceholderText("4");
    iconLayout->addRow("Border Width:", m_iconBorderWidthEdit);
    
    m_iconBorderRadiusEdit = new QLineEdit(this);
    m_iconBorderRadiusEdit->setPlaceholderText("12");
    iconLayout->addRow("Border Radius:", m_iconBorderRadiusEdit);
    
    m_iconBorderSizeEdit = new QLineEdit(this);
    m_iconBorderSizeEdit->setPlaceholderText("128");
    iconLayout->addRow("Border Size:", m_iconBorderSizeEdit);
    
    m_iconSizeEdit = new QLineEdit(this);
    m_iconSizeEdit->setPlaceholderText("120");
    iconLayout->addRow("Icon Size:", m_iconSizeEdit);
    
    m_iconColorEdit = new QLineEdit(this);
    m_iconColorEdit->setPlaceholderText("FFFFFF");
    iconLayout->addRow("Icon Color:", m_iconColorEdit);
    
    m_iconBrightnessEdit = new QLineEdit(this);
    m_iconBrightnessEdit->setPlaceholderText("100");
    iconLayout->addRow("Icon Brightness:", m_iconBrightnessEdit);
    
    m_iconOffsetEdit = new QLineEdit(this);
    m_iconOffsetEdit->setPlaceholderText("0,0");
    iconLayout->addRow("Icon Offset:", m_iconOffsetEdit);
    
    m_iconPaddingEdit = new QLineEdit(this);
    m_iconPaddingEdit->setPlaceholderText("0");
    iconLayout->addRow("Icon Padding:", m_iconPaddingEdit);
    
    tabWidget->addTab(iconTab, "Icon Settings");
    
    // Text settings tab
    QWidget *textTab = new QWidget();
    QFormLayout *textLayout = new QFormLayout(textTab);
    
    m_textColorEdit = new QLineEdit(this);
    m_textColorEdit->setPlaceholderText("FFFFFF or #00FF00");
    textLayout->addRow("Text Color:", m_textColorEdit);
    
    m_textSizeEdit = new QLineEdit(this);
    m_textSizeEdit->setPlaceholderText("16");
    textLayout->addRow("Text Size:", m_textSizeEdit);
    
    m_textFontEdit = new QLineEdit(this);
    m_textFontEdit->setPlaceholderText("dustismo_bold.ttf");
    textLayout->addRow("Text Font:", m_textFontEdit);
    
    m_textAlignEdit = new QLineEdit(this);
    m_textAlignEdit->setPlaceholderText("center, left, right");
    textLayout->addRow("Text Align:", m_textAlignEdit);
    
    m_textOffsetEdit = new QLineEdit(this);
    m_textOffsetEdit->setPlaceholderText("0,0");
    textLayout->addRow("Text Offset:", m_textOffsetEdit);
    
    tabWidget->addTab(textTab, "Text Settings");
    
    mainLayout->addWidget(tabWidget);
    
    // YAML preview
    mainLayout->addWidget(new QLabel("YAML Preview:"));
    m_yamlPreview = new QTextEdit(this);
    m_yamlPreview->setReadOnly(true);
    m_yamlPreview->setMaximumHeight(150);
    m_yamlPreview->setStyleSheet("QTextEdit { background-color: #f0f0f0; font-family: monospace; }");
    mainLayout->addWidget(m_yamlPreview);
    
    // Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(this);
    
    m_saveBtn = new QPushButton("Save to YAML", this);
    m_saveBtn->setStyleSheet("QPushButton { background-color: #2196F3; color: white; font-weight: bold; padding: 8px 16px; }");
    connect(m_saveBtn, &QPushButton::clicked, this, &PresetEditDialog::onSavePreset);
    buttonBox->addButton(m_saveBtn, QDialogButtonBox::ActionRole);
    
    QPushButton *okBtn = new QPushButton("OK", this);
    okBtn->setDefault(true);
    buttonBox->addButton(okBtn, QDialogButtonBox::AcceptRole);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    
    QPushButton *cancelBtn = new QPushButton("Cancel", this);
    buttonBox->addButton(cancelBtn, QDialogButtonBox::RejectRole);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    
    mainLayout->addWidget(buttonBox);
}

void PresetEditDialog::loadFromYaml(const QString &fullYaml)
{
    // Find presets section
    int presetsPos = fullYaml.indexOf("\npresets:");
    if (presetsPos == -1) {
        presetsPos = fullYaml.indexOf("presets:");
    }
    
    if (presetsPos == -1) {
        // No presets section
        m_beforePresets = fullYaml;
        m_afterPresets = "";
        m_presets.clear();
        populatePresetCombo();
        return;
    }
    
    m_beforePresets = fullYaml.left(presetsPos);
    
    // Find where presets section ends (next top-level key at column 0)
    int presetsEnd = -1;
    int searchStart = presetsPos + 9; // skip "presets:"
    
    for (int i = searchStart; i < fullYaml.length(); ++i) {
        if (fullYaml[i] == '\n') {
            // Check if next line is a top-level key (no leading spaces)
            int nextLineStart = i + 1;
            if (nextLineStart < fullYaml.length()) {
                // Skip empty lines
                while (nextLineStart < fullYaml.length() && fullYaml[nextLineStart] == '\n') {
                    nextLineStart++;
                }
                // Check if this line starts a top-level section
                if (nextLineStart < fullYaml.length()) {
                    QChar c = fullYaml[nextLineStart];
                    if (c.isLetter() || c == '$' || c == '_') {
                        // Check if it matches pattern: key:
                        int colonPos = fullYaml.indexOf(':', nextLineStart);
                        if (colonPos > nextLineStart) {
                            // Verify no leading spaces
                            bool hasLeadingSpace = false;
                            for (int j = i + 1; j < nextLineStart; ++j) {
                                if (fullYaml[j] == ' ' || fullYaml[j] == '\t') {
                                    hasLeadingSpace = true;
                                    break;
                                }
                            }
                            if (!hasLeadingSpace) {
                                presetsEnd = nextLineStart;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    
    if (presetsEnd == -1) {
        presetsEnd = fullYaml.length();
        m_afterPresets = "";
    } else {
        m_afterPresets = fullYaml.mid(presetsEnd);
    }
    
    // Parse the presets section
    QString presetsSection = fullYaml.mid(presetsPos, presetsEnd - presetsPos);
    parsePresetsSection(presetsSection);
    
    populatePresetCombo();
}

void PresetEditDialog::parsePresetsSection(const QString &section)
{
    m_presets.clear();
    
    QStringList lines = section.split('\n');
    QString currentPreset;
    
    for (const QString &line : lines) {
        // Check if this is a preset definition (exactly 2 spaces indent + name + colon)
        if (line.startsWith("  ") && !line.startsWith("    ")) {
            QString trimmed = line.mid(2); // Remove 2 space indent
            int colonPos = trimmed.indexOf(':');
            if (colonPos > 0 && trimmed.mid(colonPos + 1).trimmed().isEmpty()) {
                QString presetName = trimmed.left(colonPos);
                // Validate name (letters, numbers, underscore, $)
                bool valid = true;
                for (QChar c : presetName) {
                    if (!c.isLetterOrNumber() && c != '_' && c != '$') {
                        valid = false;
                        break;
                    }
                }
                if (valid && !presetName.isEmpty()) {
                    currentPreset = presetName;
                    m_presets[currentPreset] = QMap<QString, QString>();
                }
                continue;
            }
        }
        
        // Check if this is a property (4 spaces indent + key + colon + value)
        if (!currentPreset.isEmpty() && line.startsWith("    ")) {
            QString trimmed = line.mid(4); // Remove 4 space indent
            int colonPos = trimmed.indexOf(':');
            if (colonPos > 0) {
                QString key = trimmed.left(colonPos).trimmed();
                QString value = trimmed.mid(colonPos + 1).trimmed();
                // Remove quotes if present
                if ((value.startsWith('"') && value.endsWith('"')) ||
                    (value.startsWith("'") && value.endsWith("'"))) {
                    value = value.mid(1, value.length() - 2);
                }
                m_presets[currentPreset][key] = value;
            }
        }
    }
}

QString PresetEditDialog::generatePresetsSection() const
{
    if (m_presets.isEmpty()) {
        return "presets:\n";
    }
    
    QString section = "presets:\n";
    
    QStringList presetNames = m_presets.keys();
    presetNames.sort();
    
    for (const QString &name : presetNames) {
        section += QString("  %1:\n").arg(name);
        
        const QMap<QString, QString> &props = m_presets[name];
        QStringList keys = props.keys();
        keys.sort();
        
        for (const QString &key : keys) {
            QString value = props[key];
            
            // Quote values that need it
            bool needsQuotes = value.isEmpty() || 
                              value.contains(' ') || 
                              value.contains('#') ||
                              (!value[0].isDigit() && !value[0].isLetter() && value[0] != '$' && value[0] != '_' && value[0] != '-');
            
            if (needsQuotes && !value.startsWith('"')) {
                value = QString("\"%1\"").arg(value);
            }
            
            section += QString("    %1: %2\n").arg(key, value);
        }
    }
    
    return section;
}

QString PresetEditDialog::getModifiedYaml() const
{
    return m_beforePresets + generatePresetsSection() + m_afterPresets;
}

void PresetEditDialog::populatePresetCombo()
{
    m_presetCombo->clear();
    QStringList names = m_presets.keys();
    names.sort();
    for (const QString &name : names) {
        m_presetCombo->addItem(name);
    }
}

void PresetEditDialog::onPresetSelected(int index)
{
    if (index < 0) {
        m_currentPresetName.clear();
        clearUI();
        return;
    }
    
    m_currentPresetName = m_presetCombo->itemText(index);
    m_nameEdit->setText(m_currentPresetName);
    loadPresetToUI(m_currentPresetName);
}

void PresetEditDialog::loadPresetToUI(const QString &presetName)
{
    if (!m_presets.contains(presetName)) {
        clearUI();
        return;
    }
    
    const QMap<QString, QString> &props = m_presets[presetName];
    
    m_iconBgColorEdit->setText(props.value("icon_background_color", ""));
    m_iconBorderColorEdit->setText(props.value("icon_border_color", ""));
    m_iconBorderWidthEdit->setText(props.value("icon_border_width", ""));
    m_iconBorderRadiusEdit->setText(props.value("icon_border_radius", ""));
    m_iconBorderSizeEdit->setText(props.value("icon_border_size", ""));
    m_iconSizeEdit->setText(props.value("icon_size", ""));
    m_iconColorEdit->setText(props.value("icon_color", ""));
    m_iconBrightnessEdit->setText(props.value("icon_brightness", ""));
    m_iconOffsetEdit->setText(props.value("icon_offset", ""));
    m_iconPaddingEdit->setText(props.value("icon_padding", ""));
    
    m_textColorEdit->setText(props.value("text_color", ""));
    m_textSizeEdit->setText(props.value("text_size", ""));
    m_textFontEdit->setText(props.value("text_font", ""));
    m_textAlignEdit->setText(props.value("text_align", ""));
    m_textOffsetEdit->setText(props.value("text_offset", ""));
    
    updateYamlPreview();
}

void PresetEditDialog::clearUI()
{
    m_nameEdit->clear();
    m_iconBgColorEdit->clear();
    m_iconBorderColorEdit->clear();
    m_iconBorderWidthEdit->clear();
    m_iconBorderRadiusEdit->clear();
    m_iconBorderSizeEdit->clear();
    m_iconSizeEdit->clear();
    m_iconColorEdit->clear();
    m_iconBrightnessEdit->clear();
    m_iconOffsetEdit->clear();
    m_iconPaddingEdit->clear();
    
    m_textColorEdit->clear();
    m_textSizeEdit->clear();
    m_textFontEdit->clear();
    m_textAlignEdit->clear();
    m_textOffsetEdit->clear();
    
    m_yamlPreview->clear();
}

void PresetEditDialog::updateYamlPreview()
{
    QString preview = generatePresetsSection();
    QStringList lines = preview.split('\n');
    if (lines.size() > 20) {
        lines = lines.mid(0, 20);
        lines.append("...");
    }
    m_yamlPreview->setPlainText(lines.join('\n'));
}

void PresetEditDialog::onNewPreset()
{
    clearUI();
    m_presetCombo->setCurrentIndex(-1);
    m_currentPresetName.clear();
    
    // Set default values
    m_iconBgColorEdit->setText("transparent");
    m_iconBorderColorEdit->setText("FFFFFF");
    m_iconBorderWidthEdit->setText("4");
    m_iconBorderRadiusEdit->setText("12");
    m_iconBorderSizeEdit->setText("128");
    m_iconSizeEdit->setText("120");
    m_iconColorEdit->setText("FFFFFF");
    m_iconBrightnessEdit->setText("100");
    m_iconOffsetEdit->setText("0,0");
    m_iconPaddingEdit->setText("0");
    
    m_textColorEdit->setText("FFFFFF");
    m_textSizeEdit->setText("16");
    m_textAlignEdit->setText("center");
    m_textOffsetEdit->setText("0,0");
    
    m_nameEdit->setFocus();
    
    QMessageBox::information(this, "Create New Preset", 
        "Enter a name for the new preset, configure the settings, then click 'Save to YAML'.");
}

void PresetEditDialog::onDeletePreset()
{
    if (m_currentPresetName.isEmpty()) {
        QMessageBox::warning(this, "Error", "No preset selected!");
        return;
    }
    
    int ret = QMessageBox::question(this, "Delete Preset",
        QString("Are you sure you want to delete preset '%1'?").arg(m_currentPresetName),
        QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        m_presets.remove(m_currentPresetName);
        m_currentPresetName.clear();
        populatePresetCombo();
        clearUI();
        updateYamlPreview();
        
        QMessageBox::information(this, "Deleted", "Preset deleted. Click OK to confirm changes.");
    }
}

void PresetEditDialog::onSavePreset()
{
    QString presetName = m_nameEdit->text().trimmed();
    if (presetName.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please enter a preset name!");
        return;
    }
    
    // Validate preset name
    QRegularExpression validNameRe("^[a-zA-Z_$][a-zA-Z0-9_$]*$");
    if (!validNameRe.match(presetName).hasMatch()) {
        QMessageBox::warning(this, "Error", 
            "Preset name must start with a letter or underscore and contain only letters, numbers, and underscores!");
        return;
    }
    
    bool isNew = !m_presets.contains(presetName);
    
    // If renaming, remove old name
    if (!isNew && presetName != m_currentPresetName && !m_currentPresetName.isEmpty()) {
        m_presets.remove(m_currentPresetName);
    }
    
    // Save all properties
    QMap<QString, QString> props;
    
    auto addProp = [&](QLineEdit *edit, const QString &key) {
        QString value = edit->text().trimmed();
        if (!value.isEmpty()) {
            props[key] = value;
        }
    };
    
    addProp(m_iconBgColorEdit, "icon_background_color");
    addProp(m_iconBorderColorEdit, "icon_border_color");
    addProp(m_iconBorderWidthEdit, "icon_border_width");
    addProp(m_iconBorderRadiusEdit, "icon_border_radius");
    addProp(m_iconBorderSizeEdit, "icon_border_size");
    addProp(m_iconSizeEdit, "icon_size");
    addProp(m_iconColorEdit, "icon_color");
    addProp(m_iconBrightnessEdit, "icon_brightness");
    addProp(m_iconOffsetEdit, "icon_offset");
    addProp(m_iconPaddingEdit, "icon_padding");
    addProp(m_textColorEdit, "text_color");
    addProp(m_textSizeEdit, "text_size");
    addProp(m_textFontEdit, "text_font");
    addProp(m_textAlignEdit, "text_align");
    addProp(m_textOffsetEdit, "text_offset");
    
    m_presets[presetName] = props;
    m_currentPresetName = presetName;
    
    populatePresetCombo();
    m_presetCombo->setCurrentText(presetName);
    updateYamlPreview();
    
    if (isNew) {
        QMessageBox::information(this, "Created", 
            QString("New preset '%1' created! Click OK in the main dialog to save to YAML.").arg(presetName));
    } else {
        QMessageBox::information(this, "Updated", 
            QString("Preset '%1' updated! Click OK in the main dialog to save to YAML.").arg(presetName));
    }
}
