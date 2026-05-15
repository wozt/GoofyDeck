#ifndef CONFIGEDITOR_H
#define CONFIGEDITOR_H

#include <QWidget>

class QTextEdit;
class QListWidget;
class QStackedWidget;
class QLineEdit;
class QComboBox;
class YamlHighlighter;

class ConfigEditor : public QWidget
{
    Q_OBJECT

public:
    explicit ConfigEditor(QWidget *parent = nullptr);
    
    void setConfigText(const QString &text);
    QString configText() const;

signals:
    void configChanged();

private:
    void setupUI();
    
    QTextEdit *m_textEdit = nullptr;
    QListWidget *m_pageList = nullptr;
    QStackedWidget *m_editorStack = nullptr;
    YamlHighlighter *m_highlighter = nullptr;
};

#endif // CONFIGEDITOR_H
