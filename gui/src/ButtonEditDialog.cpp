#include "ButtonEditDialog.h"
#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QFileDialog>
#include <QPixmap>
#include <QDir>
#include <QMessageBox>
#include <QRegularExpression>

ButtonEditDialog::ButtonEditDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUI();
    setWindowTitle("Edit Button");
    setModal(true);
    resize(500, 450);
}

void ButtonEditDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Form layout for button properties
    QFormLayout *formLayout = new QFormLayout();
    
    // Name field
    m_nameEdit = new QLineEdit(this);
    formLayout->addRow("Name:", m_nameEdit);
    
    // Preset selector
    QHBoxLayout *presetLayout = new QHBoxLayout();
    m_presetCombo = new QComboBox(this);
    m_presetCombo->setEditable(true);
    m_presetCombo->addItem("None", "");
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &ButtonEditDialog::onPresetChanged);
    presetLayout->addWidget(m_presetCombo, 1);
    
    formLayout->addRow("Preset:", presetLayout);
    
    // Icon selector with combo and preview
    QHBoxLayout *iconLayout = new QHBoxLayout();
    
    m_iconCombo = new QComboBox(this);
    m_iconCombo->setEditable(true);
    populateIconCombo();
    connect(m_iconCombo, &QComboBox::currentTextChanged, this, &ButtonEditDialog::onIconChanged);
    iconLayout->addWidget(m_iconCombo, 1);
    
    // Manual edit field for custom icons
    m_iconEdit = new QLineEdit(this);
    m_iconEdit->setPlaceholderText("mdi:flash, url:https://..., /path/to/icon.png");
    iconLayout->addWidget(m_iconEdit, 1);
    
    // Browse button for file icons
    m_browseBtn = new QPushButton("Browse...", this);
    connect(m_browseBtn, &QPushButton::clicked, this, &ButtonEditDialog::onBrowseIcon);
    iconLayout->addWidget(m_browseBtn);
    
    formLayout->addRow("Icon:", iconLayout);
    
    // Icon preview with style frame
    QHBoxLayout *previewLayout = new QHBoxLayout();
    m_iconPreview = new QLabel(this);
    m_iconPreview->setFixedSize(80, 80);
    m_iconPreview->setAlignment(Qt::AlignCenter);
    m_iconPreview->setStyleSheet("background-color: #1a1a1a; border: 3px solid #444; border-radius: 12px;");
    previewLayout->addWidget(m_iconPreview);
    previewLayout->addStretch();
    formLayout->addRow("Preview:", previewLayout);
    
    // Action field
    m_actionEdit = new QLineEdit(this);
    m_actionEdit->setPlaceholderText("$page.go_to, $cmd.exec, light.toggle, etc.");
    formLayout->addRow("Action:", m_actionEdit);
    
    // Entity ID field
    m_entityEdit = new QLineEdit(this);
    m_entityEdit->setPlaceholderText("entity_id or command data");
    formLayout->addRow("Entity ID:", m_entityEdit);
    
    mainLayout->addLayout(formLayout);
    mainLayout->addStretch();
    
    // Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ButtonEditDialog::validateAndAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void ButtonEditDialog::populateIconCombo()
{
    m_iconCombo->clear();
    m_iconCombo->addItem("Custom...", "");
    
    // Search for MDI icons in assets/mdi/ directory
    QStringList searchPaths = {
        "../assets/mdi/",
        "../../assets/mdi/",
        "./assets/mdi/",
        QDir::home().filePath("GoofyDeck/assets/mdi/")
    };
    
    QDir mdiDir;
    for (const QString &path : searchPaths) {
        if (QDir(path).exists()) {
            mdiDir = QDir(path);
            break;
        }
    }
    
    if (mdiDir.exists()) {
        QStringList svgFiles = mdiDir.entryList(QStringList() << "*.svg", QDir::Files);
        svgFiles.sort();
        
        // Limit to first 200 icons to avoid performance issues
        int count = 0;
        for (const QString &file : svgFiles) {
            if (count >= 200) break;
            QString iconName = "mdi:" + file.left(file.length() - 4); // Remove .svg extension
            m_iconCombo->addItem(iconName, iconName);
            count++;
        }
        
        // Add a "More..." entry if there are more icons
        if (svgFiles.size() > 200) {
            m_iconCombo->addItem(QString("... (%1 more icons)").arg(svgFiles.size() - 200), "");
        }
    }
    
    m_iconCombo->setCurrentIndex(0);
}

void ButtonEditDialog::populatePresetCombo()
{
    m_presetCombo->clear();
    m_presetCombo->addItem("None", "");
    
    for (const QString &preset : m_availablePresets) {
        m_presetCombo->addItem(preset, preset);
    }
}

QPixmap ButtonEditDialog::loadIcon(const QString &iconName)
{
    if (iconName.isEmpty()) {
        return QPixmap();
    }
    
    // Handle URL icons
    if (iconName.startsWith("url:")) {
        // For URL icons, we can't easily download and show them
        // Return empty pixmap - will show placeholder
        return QPixmap();
    }
    
    // Check if it's a file path
    if (iconName.startsWith("/") || iconName.startsWith("./") || QFile::exists(iconName)) {
        // Handle SVG files - try to load as pixmap (Qt may have SVG support built-in)
        if (iconName.endsWith(".svg", Qt::CaseInsensitive)) {
            QPixmap pix(iconName);
            if (!pix.isNull()) {
                return pix.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            // SVG couldn't be loaded directly
            return QPixmap();
        } else {
            QPixmap pix(iconName);
            if (!pix.isNull()) {
                return pix.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
        }
    }
    
    // Try to load from assets/icons if it's an MDI icon
    if (iconName.startsWith("mdi:")) {
        QString iconFile = iconName.mid(4) + ".png";
        QStringList searchPaths = {
            "../assets/icons/" + iconFile,
            "../../assets/icons/" + iconFile,
            "./assets/icons/" + iconFile,
            QDir::home().filePath("GoofyDeck/assets/icons/" + iconFile)
        };
        
        for (const QString &path : searchPaths) {
            if (QFile::exists(path)) {
                QPixmap pix(path);
                if (!pix.isNull()) {
                    return pix.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
            }
        }
    }
    
    return QPixmap();
}

void ButtonEditDialog::onIconChanged(const QString &icon)
{
    // Update the edit field with the selected icon
    if (!icon.isEmpty() && icon != "Custom...") {
        m_iconEdit->setText(icon);
    }
    
    // Update preview with preset styling
    QString iconName = m_iconEdit->text();
    QString presetName = m_presetCombo->currentData().toString();
    
    QPixmap pix;
    if (!presetName.isEmpty()) {
        pix = generateStyledIcon(iconName, presetName);
    } else {
        pix = loadIcon(iconName);
    }
    
    if (!pix.isNull()) {
        m_iconPreview->setPixmap(pix);
        m_iconPreview->setText("");
        m_iconPreview->setStyleSheet("background-color: #1a1a1a; border: 3px solid #4CAF50; border-radius: 12px;");
    } else {
        m_iconPreview->setPixmap(QPixmap());
        QString displayText = iconName.isEmpty() ? "No icon" : 
                              (iconName.startsWith("url:") ? "URL icon" : "?");
        m_iconPreview->setText(displayText);
        m_iconPreview->setStyleSheet("background-color: #1a1a1a; border: 3px solid #666; "
                                     "border-radius: 12px; color: #888; font-size: 12px;");
    }
}

void ButtonEditDialog::onBrowseIcon()
{
    QString path = QFileDialog::getOpenFileName(this, "Select Icon",
        QDir::home().path(),
        "Images (*.png *.jpg *.jpeg *.bmp *.svg);;All Files (*)");
    
    if (!path.isEmpty()) {
        m_iconEdit->setText(path);
        onIconChanged(path);
    }
}

void ButtonEditDialog::setButtonConfig(int buttonNumber, const ButtonConfig &config)
{
    m_buttonNumber = buttonNumber;
    setWindowTitle(QString("Edit Button %1").arg(buttonNumber));
    
    m_nameEdit->setText(config.name);
    m_iconEdit->setText(config.icon);
    
    // Try to find the icon in the combo
    if (!config.icon.isEmpty()) {
        int index = m_iconCombo->findData(config.icon);
        if (index >= 0) {
            m_iconCombo->setCurrentIndex(index);
        } else {
            m_iconCombo->setCurrentIndex(0); // Custom
        }
    }
    
    // Update preview
    onIconChanged(config.icon);
    
    m_actionEdit->setText(config.action);
    m_entityEdit->setText(config.entityId);
}

ButtonConfig ButtonEditDialog::getButtonConfig() const
{
    ButtonConfig config;
    config.name = m_nameEdit->text();
    config.icon = m_iconEdit->text();
    config.action = m_actionEdit->text();
    config.entityId = m_entityEdit->text();
    return config;
}

int ButtonEditDialog::getButtonNumber() const
{
    return m_buttonNumber;
}

void ButtonEditDialog::onPresetChanged()
{
    // Update the preview with the new preset
    onIconChanged(m_iconEdit->text());
}

void ButtonEditDialog::setAvailablePresets(const QStringList &presets)
{
    m_availablePresets = presets;
    populatePresetCombo();
}

QStringList ButtonEditDialog::getSelectedPresets() const
{
    QString preset = m_presetCombo->currentData().toString();
    if (preset.isEmpty()) {
        return QStringList();
    }
    return QStringList() << preset;
}

QPixmap ButtonEditDialog::generateStyledIcon(const QString &iconName, const QString &presetName)
{
    // For now, just return the basic icon
    // TODO: Use C pipeline to generate styled icon
    QPixmap icon = loadIcon(iconName);
    if (!icon.isNull()) {
        return icon;
    }
    
    // Return empty pixmap
    return QPixmap();
}

void ButtonEditDialog::validateAndAccept()
{
    QString name = m_nameEdit->text().trimmed();
    QString action = m_actionEdit->text().trimmed();
    QString icon = m_iconEdit->text().trimmed();
    
    // Validate name (required)
    if (name.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Button name is required.");
        m_nameEdit->setFocus();
        return;
    }
    
    // Validate name length
    if (name.length() > 20) {
        QMessageBox::warning(this, "Validation Error", "Button name must be 20 characters or less.");
        m_nameEdit->setFocus();
        return;
    }
    
    // Validate icon format if provided
    if (!icon.isEmpty()) {
        // Check for valid icon formats
        bool validIcon = false;
        if (icon.startsWith("mdi:")) {
            validIcon = true;
        } else if (icon.startsWith("url:")) {
            validIcon = true;
        } else if (icon.startsWith("local:")) {
            validIcon = true;
        } else if (icon.startsWith("/") || icon.startsWith("./")) {
            validIcon = true;
        }
        
        if (!validIcon) {
            QMessageBox::warning(this, "Validation Error", 
                "Invalid icon format. Use:\n"
                "- mdi:icon_name\n"
                "- url:https://...\n"
                "- local:/path/to/icon\n"
                "- /absolute/path/to/icon");
            m_iconEdit->setFocus();
            return;
        }
    }
    
    // Validate action format if provided
    if (!action.isEmpty()) {
        bool validAction = false;
        if (action.startsWith("$page.")) {
            validAction = true;
        } else if (action.startsWith("$cmd.")) {
            validAction = true;
        } else if (action.contains(".")) {
            // Assume it's a Home Assistant service (e.g., light.toggle)
            validAction = true;
        }
        
        if (!validAction) {
            QMessageBox::warning(this, "Validation Error", 
                "Invalid action format. Use:\n"
                "- $page.go_to\n"
                "- $cmd.exec\n"
                "- light.toggle (Home Assistant service)");
            m_actionEdit->setFocus();
            return;
        }
    }
    
    // All validations passed
    accept();
}
