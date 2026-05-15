#ifndef PAGEEDITDIALOG_H
#define PAGEEDITDIALOG_H

#include <QDialog>
#include <QString>

class QLineEdit;
class QComboBox;
class QTextEdit;

class PageEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PageEditDialog(QWidget *parent = nullptr);
    
    void setPageConfig(const QString &pageName, const QString &wallpaperPath, const QString &layoutType);
    
    QString getPageName() const;
    QString getWallpaperPath() const;
    QString getLayoutType() const;

private:
    void setupUI();
    
    QLineEdit *m_pageNameEdit = nullptr;
    QLineEdit *m_wallpaperEdit = nullptr;
    QComboBox *m_layoutCombo = nullptr;
};

#endif // PAGEEDITDIALOG_H
