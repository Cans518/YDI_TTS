#include "ui/noticecarousel.h"
#include "ui/uitheme.h"

#include <QApplication>
#include <QDynamicPropertyChangeEvent>
#include <QEnterEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTextLayout>

namespace {
constexpr int CounterWidth = 34;
}

NoticeCarousel::NoticeCarousel(QWidget *parent)
    : QLabel(parent)
    , rotation(this)
    , transition(this)
{
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    setTextFormat(Qt::PlainText);
    setWordWrap(false);
    setFocusPolicy(Qt::TabFocus);
    setProperty("hintTitle", QStringLiteral("提示信息 · 悬停暂停，点击切换"));
    rotation.setObjectName(QStringLiteral("noticeRotation"));
    rotation.setSingleShot(true);
    connect(&rotation, &QTimer::timeout, this, &NoticeCarousel::next);
    transition.setDuration(200);
    transition.setEasingCurve(QEasingCurve::OutCubic);
    connect(&transition, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        progress = value.toReal();
        update();
    });
    connect(&transition, &QVariantAnimation::finished, this, [this] {
        outgoingText.clear();
        progress = 1;
        update();
        updateTimer();
    });
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this] {
        if (qApp->applicationState() != Qt::ApplicationActive)
            settleAnimation();
        updateTimer();
    });
    qApp->installEventFilter(this);
}

void NoticeCarousel::setMessage(const QString &text, const QString &tone)
{
    message = text;
    messageTone = tone;
    rebuildPages(false); // Incoming feedback always replaces an in-flight page immediately.
}

void NoticeCarousel::setTips(const QStringList &messages)
{
    tips.clear();
    for (const auto &tip : messages)
        if (!tip.trimmed().isEmpty() && !tips.contains(tip))
            tips.append(tip);
    rebuildPages(false);
}

void NoticeCarousel::setRotationInterval(int milliseconds)
{
    interval = qMax(40, milliseconds);
    updateTimer();
}

void NoticeCarousel::appendPages(const QString &fullText, const QString &tone, int width)
{
    if (fullText.trimmed().isEmpty())
        return;
    QString displayText = fullText;
    displayText.replace(QLatin1Char('\n'), QLatin1Char(' '));
    displayText.replace(QLatin1Char('\r'), QLatin1Char(' '));
    QTextLayout layout(displayText, font());
    QTextOption options;
    options.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(options);
    layout.beginLayout();
    for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
        line.setLineWidth(qMax(1, width));
        if (line.textLength() <= 0)
            break;
        const auto part = displayText.mid(line.textStart(), line.textLength());
        if (!part.trimmed().isEmpty())
            pages.append({ part, fullText, tone });
    }
    layout.endLayout();
}

void NoticeCarousel::rebuildPages(bool preservePage)
{
    rotation.stop();
    settleAnimation();
    pageFont = font();
    const int previousPage = preservePage ? pageIndex : 0;
    const bool critical = messageTone == "busy" || messageTone == "error";
    const auto paginate = [this, critical](int width) {
        pages.clear();
        appendPages(message, messageTone, width);
        if (!critical) {
            for (const auto &tip : tips)
                if (tip != message)
                    appendPages(tip, QStringLiteral("info"), width);
        }
    };
    paginate(contentsRect().width());
    // Determine overflow using the actual text layout, including line breaks and
    // shaping, then reserve the counter area before producing the visible pages.
    if (pages.size() > 1)
        paginate(contentsRect().width() - CounterWidth);
    if (pages.isEmpty()) {
        pageIndex = 0;
        clear();
        setToolTip({});
        update();
        return;
    }
    displayPage(qBound(0, previousPage, pageCount() - 1), false);
}

QRect NoticeCarousel::textRect() const
{
    QRect area = contentsRect();
    area.setWidth(qMax(0, area.width() - (pageCount() > 1 ? CounterWidth : 0)));
    return area;
}

void NoticeCarousel::displayPage(int index, bool animate)
{
    rotation.stop();
    const QString previousText = text();
    settleAnimation();
    pageIndex = index;
    const auto page = pages.at(pageIndex);
    setText(page.text);
    setToolTip(page.fullText);
    setAccessibleDescription(page.fullText);
    setCursor(pageCount() > 1 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    if (page.tone != visibleTone) {
        visibleTone = page.tone;
        emit toneChanged(page.tone);
    }
    emit pageChanged(pageIndex, pageCount());
    if (animate && !previousText.isEmpty() && previousText != page.text && isVisible() && !UiTheme::reducedMotion()) {
        outgoingText = previousText;
        progress = 0;
        transition.setStartValue(0.0);
        transition.setEndValue(1.0);
        transition.start();
    } else {
        updateTimer();
    }
    update();
}

void NoticeCarousel::advance(int direction)
{
    if (pageCount() < 2)
        return;
    displayPage((pageIndex + direction + pageCount()) % pageCount(), true);
}

void NoticeCarousel::next()
{
    advance(1);
}

void NoticeCarousel::settleAnimation()
{
    transition.stop();
    outgoingText.clear();
    progress = 1;
    update();
}

void NoticeCarousel::updateTimer()
{
    rotation.stop();
    if (isVisible() && pageCount() > 1 && !hovered && !hasFocus() && transition.state() != QAbstractAnimation::Running
        && qApp->applicationState() == Qt::ApplicationActive)
        rotation.start(interval);
}

void NoticeCarousel::paintEvent(QPaintEvent *)
{
    if (pages.isEmpty())
        return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setFont(font());
    painter.setPen(palette().color(QPalette::WindowText));
    const auto area = textRect();
    painter.save();
    painter.setClipRect(area);
    if (transition.state() == QAbstractAnimation::Running) {
        painter.setOpacity(1 - progress);
        painter.drawText(
            area.translated(0, -qRound(progress * area.height())), Qt::AlignLeft | Qt::AlignVCenter, outgoingText);
        painter.setOpacity(progress);
        painter.drawText(
            area.translated(0, qRound((1 - progress) * area.height())), Qt::AlignLeft | Qt::AlignVCenter, text());
    } else {
        painter.drawText(area, Qt::AlignLeft | Qt::AlignVCenter, text());
    }
    painter.restore();
    if (pageCount() > 1) {
        QFont counterFont = font();
        counterFont.setPixelSize(11);
        painter.setFont(counterFont);
        QColor color = palette().color(QPalette::WindowText);
        color.setAlpha(160);
        painter.setPen(color);
        const QRect counter(
            contentsRect().right() - CounterWidth + 5, contentsRect().top(), CounterWidth - 5, contentsRect().height());
        painter.drawText(counter, Qt::AlignCenter, QStringLiteral("%1/%2").arg(pageIndex + 1).arg(pageCount()));
    }
    if (hasFocus()) {
        painter.setPen(QPen(QColor("#79a38b"), 1, Qt::DotLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
    }
}

void NoticeCarousel::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    rebuildPages(true);
}

void NoticeCarousel::showEvent(QShowEvent *event)
{
    QLabel::showEvent(event);
    hovered = underMouse();
    rebuildPages(true);
}

void NoticeCarousel::hideEvent(QHideEvent *event)
{
    rotation.stop();
    settleAnimation();
    hovered = false;
    QLabel::hideEvent(event);
}

void NoticeCarousel::enterEvent(QEnterEvent *event)
{
    hovered = true;
    settleAnimation();
    updateTimer();
    QLabel::enterEvent(event);
}

void NoticeCarousel::leaveEvent(QEvent *event)
{
    hovered = false;
    updateTimer();
    QLabel::leaveEvent(event);
}

void NoticeCarousel::focusInEvent(QFocusEvent *event)
{
    settleAnimation();
    updateTimer();
    QLabel::focusInEvent(event);
}

void NoticeCarousel::focusOutEvent(QFocusEvent *event)
{
    QLabel::focusOutEvent(event);
    updateTimer();
}

void NoticeCarousel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && pageCount() > 1) {
        next();
        event->accept();
    } else
        QLabel::mousePressEvent(event);
}

void NoticeCarousel::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Right || event->key() == Qt::Key_Space)
        next();
    else if (event->key() == Qt::Key_Left)
        advance(-1);
    else {
        QLabel::keyPressEvent(event);
        return;
    }
    event->accept();
}

void NoticeCarousel::changeEvent(QEvent *event)
{
    QLabel::changeEvent(event);
    if (event->type() == QEvent::FontChange && font() != pageFont && !reflowPending) {
        reflowPending = true;
        QTimer::singleShot(0, this, [this] {
            reflowPending = false;
            // Re-polishing a tone briefly unsets and restores the same font.
            // Only a real font change should interrupt the slide to reflow text.
            if (font() != pageFont)
                rebuildPages(true);
        });
    }
}

bool NoticeCarousel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == qApp && event->type() == QEvent::DynamicPropertyChange
        && static_cast<QDynamicPropertyChangeEvent *>(event)->propertyName() == "reducedMotion") {
        settleAnimation();
        updateTimer();
    }
    return QLabel::eventFilter(watched, event);
}
