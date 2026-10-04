#ifndef DECKWIDGET_H
#define DECKWIDGET_H

#include <QWidget>
#include <QVector>
#include "GoofyDeckClient.h"
#include "YamlParser.h"

class ButtonWidget;
class GoofyDeckClient;
class IconCache;
class StateManager;
class HaClient;
class MainWindow;

class DeckWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DeckWidget(GoofyDeckClient *client, QWidget *parent = nullptr);
    
    void updateButtonConfig(int index, const QString &name, const QString &iconPath);
    void clearButtonConfig(int index);
    void setButtonPressed(int index, bool pressed);
    void refreshIcons();
    void refreshButtonIcon(int index);
    void setCurrentPage(const QString &pageName);
    QString getCurrentPageFromCache() const;
    
    // State management
    void setupStateTracking();
    void updateButtonStateIcon(int buttonIndex, const QString &stateValue);
    void registerButtonForStateTracking(int buttonIndex, const QString &pageName,
                                          const QString &entityId, const QStringList &allPresets);
    void addStateConfig(int buttonIndex, const QString &pageName,
                        const QString &stateValue, const StateConfig &config);

signals:
    void buttonTapped(int index);
    void buttonHeld(int index);
    void buttonLongHeld(int index);
    void buttonReleased(int index);
    void buttonRightClicked(int index, const QPoint &globalPos);
    void pageChanged(const QString &pageName);
    void requestConfigForButton(int buttonIndex, const QString &pageName);  // Request config from MainWindow

private slots:
    void onButtonTapped(int index);
    void onButtonHeld(int index);
    void onButtonLongHeld(int index);
    void onButtonReleased(int index);
    void onPageChanged(const QString &pageName, int offset);
    void onButtonStateChanged(int buttonIndex, const QString &pageName, const QString &newState);
    void onButtonTextChanged(int buttonIndex, const QString &pageName, const QString &newText);
    void onButtonNeedsRefresh(int buttonIndex, const QString &pageName);
    void onIconUpdated(int buttonIndex, const QString &pageName);

private:
    void setupUI();
    void loadIconsFromCache();
    void loadButtonIcon(int buttonIndex);
    QString getButtonIconName(int buttonIndex) const;
    void preloadStateIcons(const QString &pageName);
    
    GoofyDeckClient *m_client = nullptr;
    IconCache *m_iconCache = nullptr;
    StateManager *m_stateManager = nullptr;
    HaClient *m_haClient = nullptr;
    QVector<ButtonWidget*> m_buttons;
    QMap<int, QString> m_buttonIcons;  // buttonIndex -> iconName from config
    QString m_currentPage;
    bool m_stateTrackingEnabled = false;
    
    static constexpr int BUTTON_COUNT = 14;
};

#endif // DECKWIDGET_H
