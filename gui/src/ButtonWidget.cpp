#include "ButtonWidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QTimer>
#include <QStyle>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

ButtonWidget::ButtonWidget(int index, QWidget *parent)
    : QWidget(parent)
    , m_index(index)
    , m_holdTimer(new QTimer(this))
    , m_longHoldTimer(new QTimer(this))
{
    m_holdTimer->setSingleShot(true);
    m_holdTimer->setInterval(HOLD_THRESHOLD_MS);
    connect(m_holdTimer, &QTimer::timeout, this, &ButtonWidget::emitHold);
    
    m_longHoldTimer->setSingleShot(true);
    m_longHoldTimer->setInterval(LONGHOLD_THRESHOLD_MS);
    connect(m_longHoldTimer, &QTimer::timeout, this, &ButtonWidget::emitLongHold);
    
    setupUI();
}

void ButtonWidget::setupUI()
{
    // Button 14 is rectangular (208x110, ratio 2.08:1 = 2 buttons + gap)
    // 100px + 8px gap + 100px = 208px
    if (m_index == 14) {
        setFixedSize(208, 110);
    } else {
        setFixedSize(100, 110);
    }
    
    setStyleSheet(R"(
        ButtonWidget {
            background-color: transparent;
            border: 2px solid #444;
            border-radius: 8px;
        }
        ButtonWidget:hover {
            border-color: #666;
        }
        ButtonWidget[pressed="true"] {
            background-color: #1a1a1a;
            border-color: #00aa00;
        }
    )");
    
    // Load label style from JSON (includes alignment info)
    loadLabelStyle();
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(0);
    
    // Container widget for icon + overlaid label
    QWidget *iconContainer = new QWidget(this);
    iconContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    iconContainer->setStyleSheet("background-color: transparent;");
    QVBoxLayout *iconLayout = new QVBoxLayout(iconContainer);
    iconLayout->setContentsMargins(0, 0, 0, 0);
    iconLayout->setSpacing(0);
    
    // Icon label (fills container, centered)
    m_iconLabel = new QLabel(iconContainer);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setText(QString::number(m_index));
    m_iconLabel->setStyleSheet("color: #aaa; font-size: 24px; font-weight: bold; background: transparent;");
    iconLayout->addWidget(m_iconLabel, 1, Qt::AlignCenter);
    
    // Name label at bottom (overlaid style with semi-transparent background)
    m_nameLabel = new QLabel(iconContainer);
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setWordWrap(false);
    m_nameLabel->setStyleSheet(m_labelStyleSheet);
    m_nameLabel->setMaximumHeight(m_labelHeight);
    iconLayout->addWidget(m_nameLabel, 0, Qt::AlignBottom | Qt::AlignHCenter);
    
    layout->addWidget(iconContainer, 1, Qt::AlignCenter);
}

void ButtonWidget::loadLabelStyle()
{
    // Default style matching assets/json/default.json
    int labelSize = 12;
    int labelColor = 0xFFFFFF; // White (16777215)
    QString fontName = "Roboto";
    int fontWeight = 80;
    m_labelAlign = "bottom"; // Default alignment
    
    // Try to load from JSON file
    QStringList possiblePaths;
    possiblePaths << "../assets/json/default.json";
    possiblePaths << "../../assets/json/default.json";
    possiblePaths << "./assets/json/default.json";
    
    for (const QString &path : possiblePaths) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            file.close();
            
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isNull() && doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains("Size")) labelSize = obj["Size"].toInt(labelSize);
                if (obj.contains("Color")) labelColor = obj["Color"].toInt(labelColor);
                if (obj.contains("FontName")) fontName = obj["FontName"].toString(fontName);
                if (obj.contains("Weight")) fontWeight = obj["Weight"].toInt(fontWeight);
                if (obj.contains("Align")) m_labelAlign = obj["Align"].toString(m_labelAlign);
            }
            break;
        }
    }
    
    // Convert color int to hex string
    QColor color(labelColor);
    QString colorHex = color.name();
    
    // Calculate font weight (0-99 to QFont::Weight)
    QFont::Weight weight = QFont::Normal;
    if (fontWeight >= 70) weight = QFont::Bold;
    if (fontWeight >= 80) weight = QFont::Black;
    
    // Build stylesheet for the label
    m_labelStyleSheet = QString(
        "QLabel { "
        "  color: %1;"
        "  font-family: '%2';"
        "  font-size: %3px;"
        "  font-weight: %4;"
        "  background-color: transparent;"  // Fully transparent background
        "  padding: 2px 4px;"
        "  border-radius: 3px;"
        "}"
    ).arg(colorHex, fontName, QString::number(labelSize), 
          weight == QFont::Bold ? "bold" : (weight == QFont::Black ? "900" : "normal"));
    
    m_labelHeight = labelSize + 8; // Size + padding
}

void ButtonWidget::positionLabelOverlay()
{
    if (!m_nameLabel) return;
    
    int containerWidth = width() - 8; // Account for margins
    int containerHeight = height() - 8;
    int labelWidth = containerWidth;
    int labelHeight = m_labelHeight;
    int x = 0;
    int y = 0;
    
    // Position based on alignment from JSON
    if (m_labelAlign == "top") {
        y = 0;
    } else if (m_labelAlign == "center") {
        y = (containerHeight - labelHeight) / 2;
    } else { // bottom (default)
        y = containerHeight - labelHeight;
    }
    
    m_nameLabel->setGeometry(x, y, labelWidth, labelHeight);
}

void ButtonWidget::setButtonName(const QString &name)
{
    m_nameLabel->setText(name);
}

void ButtonWidget::setButtonIcon(const QIcon &icon)
{
    if (!icon.isNull()) {
        m_iconLabel->setPixmap(icon.pixmap(48, 48));
    }
}

void ButtonWidget::setButtonPixmap(const QPixmap &pixmap)
{
    if (!pixmap.isNull()) {
        // Clear text first, then set pixmap
        m_iconLabel->setText("");
        // Scale to fit the label while maintaining aspect ratio
        // Button 14 is 208x110, others are 100x110
        int targetWidth = (m_index == 14) ? 188 : 80;
        int targetHeight = (m_index == 14) ? 90 : 80;
        QPixmap scaled = pixmap.scaled(targetWidth, targetHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_iconLabel->setPixmap(scaled);
    }
}

void ButtonWidget::clearButtonPixmap()
{
    m_iconLabel->setPixmap(QPixmap());  // Explicitly clear pixmap
    m_iconLabel->setText(QString::number(m_index));
    m_iconLabel->setStyleSheet("color: #aaa; font-size: 24px; font-weight: bold;");
}

void ButtonWidget::setPressed(bool pressed)
{
    m_isPressed = pressed;
    setProperty("pressed", pressed);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void ButtonWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isPressed = true;
        m_holdEmitted = false;
        m_longHoldEmitted = false;
        setPressed(true);
        startHoldDetection();
    }
    QWidget::mousePressEvent(event);
}

void ButtonWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isPressed) {
        stopHoldDetection();
        setPressed(false);
        
        // Emit TAP only if no hold was emitted
        if (!m_holdEmitted && !m_longHoldEmitted) {
            emit tapped(m_index);
        }
        emit released(m_index);
        
        m_isPressed = false;
    }
    QWidget::mouseReleaseEvent(event);
}

void ButtonWidget::contextMenuEvent(QContextMenuEvent *event)
{
    // Emit signal for MainWindow to show the context menu
    emit rightClicked(event->globalPos());
}

void ButtonWidget::startHoldDetection()
{
    m_holdTimer->start();
    m_longHoldTimer->start();
}

void ButtonWidget::stopHoldDetection()
{
    m_holdTimer->stop();
    m_longHoldTimer->stop();
}

void ButtonWidget::emitHold()
{
    if (m_isPressed && !m_holdEmitted) {
        m_holdEmitted = true;
        emit held(m_index);
    }
}

void ButtonWidget::emitLongHold()
{
    if (m_isPressed && !m_longHoldEmitted) {
        m_longHoldEmitted = true;
        m_holdEmitted = true; // Prevent hold from emitting
        emit longHeld(m_index);
    }
}
