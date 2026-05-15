#ifndef PAGECREATEDIALOG_H
#define PAGECREATEDIALOG_H

#include <QDialog>
#include <QString>

class QLineEdit;
class QComboBox;
class QTextEdit;

class PageCreateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PageCreateDialog(QWidget *parent = nullptr);
    
    QString getPageName() const;
    QString getWallpaperPath() const;
    QString getLayoutType() const;

private:
    void setupUI();
    
    QLineEdit *m_pageNameEdit = nullptr;
    QLineEdit *m_wallpaperEdit = nullptr;
    QComboBox *m_layoutCombo = nullptr;
};

#endif // PAGECREATEDIALOG_H
