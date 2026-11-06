#include "ui/activityindicator.h"
#include "ui/uitheme.h"

#include <QApplication>
#include <QDynamicPropertyChangeEvent>
#include <QPainter>

ActivityIndicator::ActivityIndicator(QWidget *parent)
    : QWidget(parent)
    , m_timer(this)
{
    setFixedSize(20, 20);
    setAccessibleName(QStringLiteral("操作状态"));
    m_timer.setInterval(32);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        m_angle = (m_angle + 12) % 360;
        update();
    });
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this] { updateTimer(); });
    qApp->installEventFilter(this);
}

void ActivityIndicator::setState(const QString &state)
{
    m_state = state;
    setAccessibleDescription(state == "busy" ? QStringLiteral("正在处理")
            : state == "error"               ? QStringLiteral("操作失败")
            : state == "success"             ? QStringLiteral("操作成功")
                                             : QStringLiteral("提示"));
    m_angle = 0;
    updateTimer();
    update();
}

void ActivityIndicator::updateTimer()
{
    if (m_state == "busy" && isVisible() && !UiTheme::reducedMotion()
        && qApp->applicationState() == Qt::ApplicationActive)
        m_timer.start();
    else
        m_timer.stop();
}

void ActivityIndicator::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor color(m_state == "error" ? "#b25046" : "#268470");
    p.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (m_state == "busy") {
        p.setPen(QPen(QColor("#c5ddd3"), 2));
        p.drawEllipse(QRectF(3, 3, 14, 14));
        p.setPen(QPen(color, 2, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(QRectF(3, 3, 14, 14), -m_angle * 16, 245 * 16);
    } else {
        p.drawEllipse(QRectF(3, 3, 14, 14));
        if (m_state == "success") {
            p.drawPolyline(QPolygonF { QPointF(6, 10), QPointF(9, 13), QPointF(14, 7) });
        } else {
            p.drawLine(QPointF(10, 7), QPointF(10, 10));
            p.drawPoint(QPointF(10, 13));
        }
    }
}

void ActivityIndicator::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateTimer();
}
void ActivityIndicator::hideEvent(QHideEvent *event)
{
    m_timer.stop();
    QWidget::hideEvent(event);
}
bool ActivityIndicator::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == qApp && event->type() == QEvent::DynamicPropertyChange
        && static_cast<QDynamicPropertyChangeEvent *>(event)->propertyName() == "reducedMotion") {
        updateTimer();
        update();
    }
    return QWidget::eventFilter(watched, event);
}
