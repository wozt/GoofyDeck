#include "ConfigEditor.h"
#include "YamlHighlighter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QListWidget>
#include <QStackedWidget>
#include <QLabel>

ConfigEditor::ConfigEditor(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void ConfigEditor::setupUI()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // Left panel: Page list
    QVBoxLayout *leftPanel = new QVBoxLayout();
    leftPanel->addWidget(new QLabel("Pages:"));
    
    m_pageList = new QListWidget(this);
    leftPanel->addWidget(m_pageList);
    
    mainLayout->addLayout(leftPanel, 1);
    
    // Right panel: Text editor
    QVBoxLayout *rightPanel = new QVBoxLayout();
    rightPanel->addWidget(new QLabel("Configuration YAML:"));
    
    m_textEdit = new QTextEdit(this);
    m_textEdit->setFont(QFont("Consolas", 10));
    
    // Set dark theme for editor
    m_textEdit->setStyleSheet(R"(
        QTextEdit {
            background-color: #1e1e1e;
            color: #d4d4d4;
            border: 1px solid #444;
        }
    )");
    
    // Setup YAML syntax highlighting
    m_highlighter = new YamlHighlighter(m_textEdit->document());
    
    connect(m_textEdit, &QTextEdit::textChanged, this, &ConfigEditor::configChanged);
    
    rightPanel->addWidget(m_textEdit, 1);
    
    mainLayout->addLayout(rightPanel, 3);
}

void ConfigEditor::setConfigText(const QString &text)
{
    m_textEdit->setPlainText(text);
}

QString ConfigEditor::configText() const
{
    return m_textEdit->toPlainText();
}
