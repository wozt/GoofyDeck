#include "PageEditDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QDialogButtonBox>

PageEditDialog::PageEditDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUI();
    setWindowTitle("Edit Page");
    setModal(true);
    resize(400, 200);
}

void PageEditDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Form layout for page properties
    QFormLayout *formLayout = new QFormLayout();
    
    m_pageNameEdit = new QLineEdit(this);
    m_pageNameEdit->setPlaceholderText("e.g., my_page");
    m_pageNameEdit->setReadOnly(true); // Page name cannot be changed
    
    m_wallpaperEdit = new QLineEdit(this);
    m_wallpaperEdit->setPlaceholderText("path/to/wallpaper.png");
    
    m_layoutCombo = new QComboBox(this);
    m_layoutCombo->addItem("3x5 (Standard)", "3x5");
    m_layoutCombo->addItem("1x1 (Single Button)", "1x1");
    
    formLayout->addRow("Page Name:", m_pageNameEdit);
    formLayout->addRow("Wallpaper Path:", m_wallpaperEdit);
    formLayout->addRow("Layout:", m_layoutCombo);
    
    mainLayout->addLayout(formLayout);
    
    // Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void PageEditDialog::setPageConfig(const QString &pageName, const QString &wallpaperPath, const QString &layoutType)
{
    setWindowTitle(QString("Edit Page '%1'").arg(pageName));
    m_pageNameEdit->setText(pageName);
    m_wallpaperEdit->setText(wallpaperPath);
    
    // Select the correct layout in the combo box
    int index = m_layoutCombo->findData(layoutType);
    if (index >= 0) {
        m_layoutCombo->setCurrentIndex(index);
    }
}

QString PageEditDialog::getPageName() const
{
    return m_pageNameEdit->text();
}

QString PageEditDialog::getWallpaperPath() const
{
    return m_wallpaperEdit->text();
}

QString PageEditDialog::getLayoutType() const
{
    return m_layoutCombo->currentData().toString();
}
