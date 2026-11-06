#include "ui/animatedbutton.h"
#include "ui/uitheme.h"

#include <QApplication>
#include <QDynamicPropertyChangeEvent>
#include <QEnterEvent>
#include <QPainter>

namespace {
QColor blend(const QColor &a, const QColor &b, qreal t)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
        a.blueF() + (b.blueF() - a.blueF()) * t);
}
}

AnimatedButton::AnimatedButton(const QString &text, Tone tone, QWidget *parent)
    : QPushButton(text, parent)
    , m_tone(tone)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setFixedHeight(28);
    m_hoverAnimation.setDuration(140);
    m_hoverAnimation.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_hoverAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_hover = value.toReal();
        update();
    });
    qApp->installEventFilter(this);
}

void AnimatedButton::setTone(Tone tone)
{
    m_tone = tone;
    update();
}

QSize AnimatedButton::sizeHint() const
{
    const int content = fontMetrics().horizontalAdvance(text()) + (icon().isNull() ? 0 : 24);
    return QSize(qMax(28, content + (text().isEmpty() ? 4 : 24)), 28);
}

void AnimatedButton::paintEvent(QPaintEvent *)
{
    const bool accented = m_tone != Tone::Secondary && m_tone != Tone::Quiet;
    QColor background;
    QColor foreground = UiTheme::Colors::Text;
    switch (m_tone) {
    case Tone::Primary:
        background = UiTheme::Colors::Blue;
        foreground = UiTheme::Colors::BlueText;
        break;
    case Tone::Success:
        background = UiTheme::Colors::Green;
        foreground = UiTheme::Colors::GreenText;
        break;
    case Tone::Warning:
        background = UiTheme::Colors::Amber;
        foreground = UiTheme::Colors::AmberText;
        break;
    case Tone::Danger:
        background = UiTheme::Colors::Red;
        foreground = UiTheme::Colors::RedText;
        break;
    case Tone::Volume:
        background = UiTheme::Colors::Purple;
        foreground = UiTheme::Colors::PurpleText;
        break;
    case Tone::Speed:
        background = UiTheme::Colors::Teal;
        foreground = UiTheme::Colors::TealText;
        break;
    case Tone::Secondary:
        background = UiTheme::Colors::Soft;
        break;
    case Tone::Quiet:
        background = UiTheme::Colors::Surface;
        break;
    }
    background
        = blend(background, accented ? blend(background, foreground, 0.07) : UiTheme::Colors::SoftHover, m_hover);
    if (isDown())
        background = blend(background, foreground, accented ? 0.10 : 0.06);
    QColor border = accented ? blend(background, foreground, 0.22)
                             : blend(UiTheme::Colors::Border, UiTheme::Colors::Focus, m_hover);
    if (!isEnabled()) {
        background = UiTheme::Colors::DisabledSurface;
        foreground = UiTheme::Colors::DisabledText;
        border = UiTheme::Colors::Soft;
    }
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const QRectF bounds = QRectF(rect()).adjusted(2, 2, -2, -2);
    if (hasFocus() && isEnabled()) {
        p.setPen(QPen(UiTheme::Colors::Focus, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(rect()).adjusted(0.8, 0.8, -0.8, -0.8), 8, 8);
    }
    if (isEnabled() && !isDown()) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(40, 57, 37, 10));
        p.drawRoundedRect(bounds.translated(0, 1), 6, 6);
    }
    QLinearGradient surface(bounds.topLeft(), bounds.bottomLeft());
    surface.setColorAt(0, isEnabled() && !isDown() ? blend(background, UiTheme::Colors::Surface, 0.04) : background);
    surface.setColorAt(1, background);
    p.setBrush(surface);
    p.setPen(QPen(border, 1));
    p.drawRoundedRect(bounds, 6, 6);

    const int iconWidth = icon().isNull() ? 0 : 20;
    const int gap = iconWidth && !text().isEmpty() ? 6 : 0;
    const int width = fontMetrics().horizontalAdvance(text()) + iconWidth + gap;
    int x = (this->width() - width) / 2;
    const int offset = isDown() ? 1 : 0;
    if (iconWidth) {
        p.setOpacity(isEnabled() ? 1.0 : 0.45);
        icon().paint(&p, QRect(x, (height() - 20) / 2 + offset, 20, 20));
        p.setOpacity(1.0);
        x += iconWidth + gap;
    }
    p.setPen(foreground);
    p.setFont(font());
    p.drawText(QRect(x, offset, this->width() - x, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
}

void AnimatedButton::animateHover(qreal target)
{
    m_hoverAnimation.stop();
    if (UiTheme::reducedMotion() || !isVisible() || !isEnabled()) {
        m_hover = isEnabled() ? target : 0;
        update();
        return;
    }
    m_hoverAnimation.setStartValue(m_hover);
    m_hoverAnimation.setEndValue(target);
    m_hoverAnimation.start();
}

void AnimatedButton::enterEvent(QEnterEvent *event)
{
    animateHover(1);
    QPushButton::enterEvent(event);
}
void AnimatedButton::leaveEvent(QEvent *event)
{
    animateHover(0);
    QPushButton::leaveEvent(event);
}
void AnimatedButton::hideEvent(QHideEvent *event)
{
    m_hoverAnimation.stop();
    m_hover = 0;
    QPushButton::hideEvent(event);
}
void AnimatedButton::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::EnabledChange)
        animateHover(0);
    QPushButton::changeEvent(event);
}
bool AnimatedButton::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == qApp && event->type() == QEvent::DynamicPropertyChange
        && static_cast<QDynamicPropertyChangeEvent *>(event)->propertyName() == "reducedMotion")
        animateHover(underMouse() ? 1 : 0);
    return QPushButton::eventFilter(watched, event);
}
