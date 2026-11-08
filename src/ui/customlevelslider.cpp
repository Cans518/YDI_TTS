#include "ui/customlevelslider.h"
#include "ui/uitheme.h"

#include <QApplication>
#include <QDynamicPropertyChangeEvent>
#include <QMouseEvent>
#include <QPainter>

CustomLevelSlider::CustomLevelSlider(QWidget *parent)
    : QSlider(Qt::Horizontal, parent)
{
    setRange(0, 4);
    setSingleStep(1);
    setPageStep(1);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setSliderHeight(44);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    m_animation.setDuration(160);
    m_animation.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &position) {
        m_position = position.toReal();
        update();
    });
    connect(this, &QSlider::valueChanged, this, [this](int level) {
        animateToValue();
        emit levelChanged(level);
    });
    connect(this, &QSlider::rangeChanged, this, [this] { settleAnimation(); });
    qApp->installEventFilter(this);
}

void CustomLevelSlider::setLevels(int count)
{
    setRange(0, qMax(1, count) - 1);
    settleAnimation();
}

void CustomLevelSlider::setSliderHeight(int height)
{
    setFixedHeight(qBound(28, height, 80));
    update();
}

qreal CustomLevelSlider::targetPosition() const
{
    return maximum() == minimum() ? 0 : qreal(qint64(value()) - minimum()) / (qint64(maximum()) - minimum());
}

QRectF CustomLevelSlider::trackRect() const
{
    const qreal margin = qMin(18.0, width() / 2.0);
    return QRectF(margin, height() / 2.0 - 3, qMax(0.0, width() - 2 * margin), 6);
}

void CustomLevelSlider::animateToValue()
{
    m_animation.stop();
    if (UiTheme::reducedMotion() || !isVisible() || !isEnabled() || isSliderDown()) {
        settleAnimation();
        return;
    }
    m_animation.setStartValue(m_position);
    m_animation.setEndValue(targetPosition());
    m_animation.start();
}

void CustomLevelSlider::settleAnimation()
{
    m_animation.stop();
    m_position = targetPosition();
    update();
}

void CustomLevelSlider::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const QRectF track = trackRect();
    const qreal position = isRightToLeft() ? 1.0 - m_position : m_position;
    const qreal handleX = track.left() + track.width() * position;
    const qreal centerY = track.center().y();
    p.setPen(Qt::NoPen);
    p.setBrush(UiTheme::Colors::Soft);
    p.drawRoundedRect(track, 3, 3);

    QLinearGradient gradient(track.topLeft(), track.topRight() + QPointF(1, 0));
    gradient.setColorAt(0, isEnabled() ? UiTheme::Colors::Focus : QColor("#bccfc3"));
    gradient.setColorAt(1, isEnabled() ? UiTheme::Colors::Accent : QColor("#9fb9aa"));
    p.setBrush(gradient);
    const QRectF active(
        isRightToLeft() ? handleX : track.left(), track.top(), track.width() * m_position, track.height());
    p.drawRoundedRect(active, 3, 3);

    // Cap tick density for callers that choose unusually large ranges.
    const qint64 levels = qint64(maximum()) - minimum() + 1;
    if (levels <= 20) {
        for (int i = 0; i < levels; ++i) {
            qreal fraction = levels <= 1 ? 0 : qreal(i) / (levels - 1);
            if (isRightToLeft())
                fraction = 1 - fraction;
            p.setBrush(i + minimum() <= value() ? QColor("#d7ece2") : QColor("#a9c2b5"));
            p.drawEllipse(QPointF(track.left() + fraction * track.width(), centerY), 2, 2);
        }
    }

    if (hasFocus()) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(UiTheme::Colors::Focus, 1.5));
        p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 8, 8);
    }
    const qreal radius = qMin(12.0, height() / 2.0 - 5);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(30, 80, 60, 20));
    p.drawEllipse(QPointF(handleX, centerY + 2), radius + 1, radius + 1);
    p.setPen(QPen(isEnabled() ? UiTheme::Colors::Accent : UiTheme::Colors::Border, 1.6));
    p.setBrush(UiTheme::Colors::Surface);
    p.drawEllipse(QPointF(handleX, centerY), radius, radius);
    p.setPen(isEnabled() ? UiTheme::Colors::Accent : UiTheme::Colors::DisabledText);
    QFont numberFont = font();
    numberFont.setPixelSize(12);
    numberFont.setBold(false);
    p.setFont(numberFont);
    p.drawText(QRectF(handleX - radius, centerY - radius, radius * 2, radius * 2), Qt::AlignCenter,
        QString::number(qint64(value()) + 1));
}

void CustomLevelSlider::updateLevelFromPosition(const QPointF &pos)
{
    const QRectF track = trackRect();
    if (track.width() <= 0 || maximum() == minimum()) {
        setValue(minimum());
        return;
    }
    qreal fraction = qBound(0.0, (pos.x() - track.left()) / track.width(), 1.0);
    if (isRightToLeft())
        fraction = 1 - fraction;
    // QSlider permits the full int range; interpolate in 64 bits before narrowing.
    setValue(int(qint64(minimum()) + qRound64(fraction * (qint64(maximum()) - minimum()))));
}

void CustomLevelSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    setFocus(Qt::MouseFocusReason);
    // Mark the drag before changing value so the handle follows the first press immediately.
    setSliderDown(true);
    settleAnimation(); // A press at the current value does not emit valueChanged.
    updateLevelFromPosition(event->position());
    event->accept();
}

void CustomLevelSlider::mouseMoveEvent(QMouseEvent *event)
{
    if (isSliderDown() && event->buttons().testFlag(Qt::LeftButton)) {
        updateLevelFromPosition(event->position());
        event->accept();
    } else
        event->ignore();
}

void CustomLevelSlider::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && isSliderDown()) {
        updateLevelFromPosition(event->position());
        setSliderDown(false);
        event->accept();
    } else
        event->ignore();
}

void CustomLevelSlider::hideEvent(QHideEvent *event)
{
    setSliderDown(false);
    settleAnimation();
    QSlider::hideEvent(event);
}

void CustomLevelSlider::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::EnabledChange && !isEnabled()) {
        setSliderDown(false);
        settleAnimation();
    }
    QSlider::changeEvent(event);
}

bool CustomLevelSlider::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == qApp && event->type() == QEvent::DynamicPropertyChange
        && static_cast<QDynamicPropertyChangeEvent *>(event)->propertyName() == "reducedMotion")
        settleAnimation();
    return QSlider::eventFilter(watched, event);
}
