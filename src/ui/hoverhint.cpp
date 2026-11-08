#include "ui/hoverhint.h"
#include "ui/uitheme.h"

#include <QAbstractButton>
#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QLabel>
#include <QPainter>
#include <QPointer>
#include <QPropertyAnimation>
#include <QScreen>
#include <QTimer>
#include <QToolTip>

namespace {
class HintBubble final : public QWidget
{
public:
    explicit HintBubble(QWidget *parent)
        : QWidget(parent, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus)
        , fade(this)
    {
        setObjectName(QStringLiteral("hintBubble"));
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(15, 12, 15, 12);
        layout->setSpacing(5);
        layout->setSizeConstraint(QLayout::SetFixedSize);
        auto *heading = new QHBoxLayout;
        heading->setSpacing(12);
        title = new QLabel(this);
        title->setObjectName(QStringLiteral("hintTitle"));
        title->setTextFormat(Qt::PlainText);
        shortcut = new QLabel(this);
        shortcut->setObjectName(QStringLiteral("hintShortcut"));
        shortcut->setTextFormat(Qt::PlainText);
        heading->addWidget(title);
        heading->addStretch();
        heading->addWidget(shortcut);
        layout->addLayout(heading);
        body = new QLabel(this);
        body->setObjectName(QStringLiteral("hintBody"));
        body->setTextFormat(Qt::PlainText);
        body->setWordWrap(true);
        layout->addWidget(body);
        opacity = new QGraphicsOpacityEffect(this);
        setGraphicsEffect(opacity);
        fade.setTargetObject(opacity);
        fade.setPropertyName("opacity");
        fade.setDuration(120);
        fade.setEasingCurve(QEasingCurve::OutCubic);
    }

    void present(QWidget *anchor, const QPoint &globalPosition)
    {
        QString heading = anchor->property("hintTitle").toString();
        if (heading.isEmpty()) {
            if (const auto *button = qobject_cast<QAbstractButton *>(anchor))
                heading = button->accessibleName().isEmpty() ? button->text() : button->accessibleName();
        }
        title->setText(heading);
        title->setVisible(!heading.isEmpty());
        shortcut->setText(anchor->property("hintShortcut").toString());
        shortcut->setVisible(!shortcut->text().isEmpty());
        body->setText(anchor->toolTip());
        const int headingWidth
            = title->sizeHint().width() + (!shortcut->isHidden() ? shortcut->sizeHint().width() + 16 : 0);
        const int textWidth = body->fontMetrics().horizontalAdvance(body->text());
        body->setFixedWidth(qBound(150, qMax(textWidth, headingWidth), 270));
        setAccessibleName(heading);
        setAccessibleDescription(anchor->toolTip());
        adjustSize();

        auto *screen = QGuiApplication::screenAt(globalPosition);
        if (!screen)
            screen = anchor->screen();
        const QRect available = screen->availableGeometry().adjusted(6, 6, -6, -6);
        QPoint position = globalPosition + QPoint(12, 18);
        if (position.y() + height() > available.bottom())
            position.setY(globalPosition.y() - height() - 10);
        position.setX(qBound(available.left(), position.x(), qMax(available.left(), available.right() - width() + 1)));
        position.setY(qBound(available.top(), position.y(), qMax(available.top(), available.bottom() - height() + 1)));
        move(position);
        fade.stop();
        const bool animate = !UiTheme::reducedMotion() && !isVisible();
        opacity->setOpacity(animate ? 0.0 : 1.0);
        show();
        if (animate) {
            fade.setStartValue(0.0);
            fade.setEndValue(1.0);
            fade.start();
        }
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        for (int spread = 3; spread >= 1; --spread) {
            painter.setBrush(QColor(40, 57, 37, 10));
            painter.drawRoundedRect(QRectF(rect()).adjusted(4 - spread, 5 - spread, -4 + spread, -3 + spread), 11, 11);
        }
        QLinearGradient gradient(0, 0, 0, height());
        gradient.setColorAt(0, UiTheme::Colors::Surface);
        gradient.setColorAt(1, UiTheme::Colors::SurfaceShade);
        painter.setBrush(gradient);
        painter.setPen(QPen(UiTheme::Colors::Border, 1));
        painter.drawRoundedRect(QRectF(rect()).adjusted(3.5, 3.5, -3.5, -3.5), 9, 9);
    }

    void hideEvent(QHideEvent *event) override
    {
        fade.stop();
        opacity->setOpacity(1);
        QWidget::hideEvent(event);
    }

private:
    QLabel *title;
    QLabel *body;
    QLabel *shortcut;
    QGraphicsOpacityEffect *opacity;
    QPropertyAnimation fade;
};

class HoverHintFilter final : public QObject
{
public:
    explicit HoverHintFilter(QApplication &app)
        : QObject(&app)
        , expiry(this)
    {
        setObjectName(QStringLiteral("hoverHintFilter"));
        expiry.setSingleShot(true);
        connect(&expiry, &QTimer::timeout, this, [this] { dismiss(); });
        app.installEventFilter(this);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::ToolTip) {
            auto *widget = qobject_cast<QWidget *>(watched);
            if (!widget || widget->toolTip().isEmpty())
                return false;
            auto *owner = widget->window();
            if (owner->windowType() == Qt::ToolTip)
                return false;
            if (!bubble || bubble->parentWidget() != owner) {
                if (bubble) {
                    bubble->hide();
                    bubble->deleteLater();
                }
                bubble = new HintBubble(owner);
            }
            QToolTip::hideText();
            if (source != widget) {
                QObject::disconnect(sourceDestroyed);
                source = widget;
                sourceDestroyed = connect(widget, &QObject::destroyed, this, [this] { dismiss(); });
            }
            bubble->present(widget, static_cast<QHelpEvent *>(event)->globalPos());
            expiry.start(8000);
            event->accept();
            return true;
        }
        if (source) {
            const auto type = event->type();
            if (type == QEvent::MouseButtonPress || type == QEvent::KeyPress || type == QEvent::Wheel
                || type == QEvent::ApplicationDeactivate
                || (watched == source
                    && (type == QEvent::Leave || type == QEvent::ToolTipChange || type == QEvent::Hide))
                || (watched == source->window()
                    && (type == QEvent::Hide || type == QEvent::Close || type == QEvent::WindowDeactivate)))
                dismiss();
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void dismiss()
    {
        expiry.stop();
        QObject::disconnect(sourceDestroyed);
        if (bubble)
            bubble->hide();
        source.clear();
    }
    QPointer<HintBubble> bubble;
    QPointer<QWidget> source;
    QMetaObject::Connection sourceDestroyed;
    QTimer expiry;
};
}

void installHoverHints(QApplication &app)
{
    if (!app.findChild<QObject *>(QStringLiteral("hoverHintFilter")))
        new HoverHintFilter(app);
}
