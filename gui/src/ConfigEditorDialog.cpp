#include "ConfigEditorDialog.h"
#include "YamlHighlighter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFile>
#include <QMessageBox>
#include <QFont>

ConfigEditorDialog::ConfigEditorDialog(QWidget *parent)
    : QDialog(parent)
    , m_editor(nullptr)
    , m_saveBtn(nullptr)
    , m_reloadBtn(nullptr)
    , m_closeBtn(nullptr)
    , m_statusLabel(nullptr)
    , m_highlighter(nullptr)
{
    setupUI();
    setWindowTitle("Configuration Editor");
    resize(800, 600);
}

ConfigEditorDialog::~ConfigEditorDialog()
{
}

void ConfigEditorDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // Status bar at top
    QHBoxLayout *statusLayout = new QHBoxLayout();
    m_statusLabel = new QLabel("Ready", this);
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    mainLayout->addLayout(statusLayout);

    // Editor
    m_editor = new QTextEdit(this);
    m_editor->setFont(QFont("Consolas", 10));
    m_highlighter = new YamlHighlighter(m_editor->document());
    mainLayout->addWidget(m_editor, 1);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_saveBtn = new QPushButton("Save", this);
    m_saveBtn->setStyleSheet("background-color: #4CAF50; color: white; padding: 8px 16px;");
    connect(m_saveBtn, &QPushButton::clicked, this, &ConfigEditorDialog::onSave);
    buttonLayout->addWidget(m_saveBtn);

    m_reloadBtn = new QPushButton("Reload", this);
    m_reloadBtn->setStyleSheet("background-color: #2196F3; color: white; padding: 8px 16px;");
    connect(m_reloadBtn, &QPushButton::clicked, this, &ConfigEditorDialog::onReload);
    buttonLayout->addWidget(m_reloadBtn);

    m_closeBtn = new QPushButton("Close", this);
    m_closeBtn->setStyleSheet("padding: 8px 16px;");
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(m_closeBtn);

    mainLayout->addLayout(buttonLayout);
}

void ConfigEditorDialog::setConfigContent(const QString &content)
{
    m_editor->setPlainText(content);
}

void ConfigEditorDialog::setConfigPath(const QString &path)
{
    m_configPath = path;
}

void ConfigEditorDialog::onSave()
{
    if (m_configPath.isEmpty()) {
        QMessageBox::warning(this, "Error", "No configuration path set!");
        return;
    }

    QFile file(m_configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "Failed to open configuration file for writing!");
        return;
    }

    file.write(m_editor->toPlainText().toUtf8());
    file.close();

    m_statusLabel->setText("Saved: " + m_configPath);
    m_statusLabel->setStyleSheet("color: #4CAF50;");

    emit configSaved(m_editor->toPlainText());

    QMessageBox::information(this, "Success", "Configuration saved successfully!");
}

void ConfigEditorDialog::onReload()
{
    if (m_configPath.isEmpty()) {
        QMessageBox::warning(this, "Error", "No configuration path set!");
        return;
    }

    QFile file(m_configPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "Failed to open configuration file for reading!");
        return;
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    m_editor->setPlainText(content);
    m_statusLabel->setText("Reloaded: " + m_configPath);
    m_statusLabel->setStyleSheet("color: #2196F3;");
}
