#ifndef CONFIGEDITORDIALOG_H
#define CONFIGEDITORDIALOG_H

#include <QDialog>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>

class YamlHighlighter;

class ConfigEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConfigEditorDialog(QWidget *parent = nullptr);
    ~ConfigEditorDialog();

    void setConfigContent(const QString &content);
    void setConfigPath(const QString &path);

signals:
    void configSaved(const QString &newContent);

private slots:
    void onSave();
    void onReload();

private:
    void setupUI();

    QTextEdit *m_editor;
    QPushButton *m_saveBtn;
    QPushButton *m_reloadBtn;
    QPushButton *m_closeBtn;
    QLabel *m_statusLabel;
    YamlHighlighter *m_highlighter;
    QString m_configPath;
};

#endif // CONFIGEDITORDIALOG_H
