#ifndef BUTTONEDITDIALOG_H
#define BUTTONEDITDIALOG_H

#include <QDialog>
#include <QString>

class QLineEdit;
class QComboBox;
class QLabel;
class QPushButton;

struct ButtonConfig;

class ButtonEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ButtonEditDialog(QWidget *parent = nullptr);
    
    void setButtonConfig(int buttonNumber, const ButtonConfig &config);
    ButtonConfig getButtonConfig() const;
    int getButtonNumber() const;

    void setAvailablePresets(const QStringList &presets);
    QStringList getSelectedPresets() const;

private slots:
    void onIconChanged(const QString &icon);
    void onBrowseIcon();
    void onPresetChanged();
    void validateAndAccept();

private:
    void setupUI();
    void populateIconCombo();
    void populatePresetCombo();
    QPixmap loadIcon(const QString &iconName);
    QPixmap generateStyledIcon(const QString &iconName, const QString &presetName);
    
    int m_buttonNumber = 0;
    QLineEdit *m_nameEdit = nullptr;
    QComboBox *m_iconCombo = nullptr;
    QLineEdit *m_iconEdit = nullptr;
    QLabel *m_iconPreview = nullptr;
    QPushButton *m_browseBtn = nullptr;
    QLineEdit *m_actionEdit = nullptr;
    QLineEdit *m_entityEdit = nullptr;
    QComboBox *m_presetCombo = nullptr;
    QStringList m_availablePresets;
    QStringList m_selectedPresets;
};

#endif // BUTTONEDITDIALOG_H
