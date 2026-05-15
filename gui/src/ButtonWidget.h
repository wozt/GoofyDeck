#ifndef BUTTONWIDGET_H
#define BUTTONWIDGET_H

#include <QWidget>
#include "GoofyDeckClient.h"

class QLabel;
class QPushButton;

class ButtonWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ButtonWidget(int index, QWidget *parent = nullptr);
    
    void setButtonName(const QString &name);
    void setButtonIcon(const QIcon &icon);
    void setButtonPixmap(const QPixmap &pixmap);
    void clearButtonPixmap();
    void setPressed(bool pressed);
    
    int index() const { return m_index; }

signals:
    void tapped(int index);
    void held(int index);
    void longHeld(int index);
    void released(int index);
    void rightClicked(const QPoint &globalPos);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    int m_index;
    QLabel *m_iconLabel = nullptr;
    QLabel *m_nameLabel = nullptr;
    QPushButton *m_button = nullptr;
    
    QTimer *m_holdTimer = nullptr;
    QTimer *m_longHoldTimer = nullptr;
    bool m_isPressed = false;
    bool m_holdEmitted = false;
    bool m_longHoldEmitted = false;
    
    static constexpr int HOLD_THRESHOLD_MS = 750;
    static constexpr int LONGHOLD_THRESHOLD_MS = 5000;
    
    QString m_labelStyleSheet;
    QString m_labelAlign;
    int m_labelHeight = 20;
    
    void setupUI();
    void loadLabelStyle();
    void positionLabelOverlay();
    void startHoldDetection();
    void stopHoldDetection();
    void emitHold();
    void emitLongHold();
};

#endif // BUTTONWIDGET_H
