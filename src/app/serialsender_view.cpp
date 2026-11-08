#include "app/serialsender.h"
#include "core/ttsprotocol.h"
#include "ui/activityindicator.h"
#include "ui/animatedbutton.h"
#include "ui/levelsliderwidget.h"
#include "ui/noticecarousel.h"
#include "ui/parameterpopup.h"
#include "ui/uitheme.h"

#include <QAccessible>
#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QFrame>
#include <QGraphicsBlurEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QShortcut>
#include <QStyle>
#include <QUrl>
#include <QWindow>

namespace {
QPixmap makeFrostedBackground(const QPixmap &background, const QSize &size, qreal ratio)
{
    QGraphicsScene scene;
    const QRectF bounds(QPointF(), size);
    scene.setSceneRect(bounds);
    auto *image = scene.addPixmap(background);
    const QSizeF imageSize = background.deviceIndependentSize();
    image->setPos((size.width() - imageSize.width()) / 2, (size.height() - imageSize.height()) / 2);
    auto *blur = new QGraphicsBlurEffect;
    blur->setBlurRadius(8.0 * ratio);
    blur->setBlurHints(QGraphicsBlurEffect::QualityHint);
    image->setGraphicsEffect(blur);

    QPixmap result((QSizeF(size) * ratio).toSize());
    result.setDevicePixelRatio(ratio);
    result.fill(QColor("#deded6"));
    QPainter painter(&result);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    scene.render(&painter, bounds, bounds);
    return result;
}

void repolish(QWidget *widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

}

void SerialSender::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 6, 12, 6);
    root->setSpacing(6);

    auto *header = new QHBoxLayout;
    header->setSpacing(10);
    auto *headline = new QLabel(QStringLiteral("长风 TTS 调试工具"), this);
    headline->setObjectName(QStringLiteral("headline"));
    headline->setToolTip(QStringLiteral("按住标题区域拖动窗口"));
    header->addWidget(headline);
    auto *headerLinks = new QHBoxLayout;
    headerLinks->setSpacing(4);
    const auto addLinkButton = [this, headerLinks](const QString &name, const QString &title, const QString &iconPath,
                                   const QString &description, const QUrl &url) {
        auto *button = new AnimatedButton({}, AnimatedButton::Tone::Quiet, this);
        button->setObjectName(name);
        button->setFixedSize(28, 24);
        button->setIcon(QIcon(iconPath));
        button->setIconSize(QSize(20, 20));
        button->setAccessibleName(title);
        button->setAccessibleDescription(description);
        button->setToolTip(description);
        connect(button, &QPushButton::clicked, this, [this, title, url] {
            if (!QDesktopServices::openUrl(url))
                showFeedback(QStringLiteral("无法打开%1，请检查默认浏览器设置。").arg(title), QStringLiteral("error"));
        });
        headerLinks->addWidget(button);
        return button;
    };
    auto *bilibiliButton = addLinkButton(QStringLiteral("bilibiliButton"), QStringLiteral("B站视频"),
        QStringLiteral(":/icons/bilibili.svg"), QStringLiteral("在浏览器观看长风语音播报模块的介绍视频。"),
        QUrl(QStringLiteral("https://b23.tv/CDQRwvy")));
    auto *taobaoButton = addLinkButton(QStringLiteral("taobaoButton"), QStringLiteral("淘宝商品"),
        QStringLiteral(":/icons/taobao.svg"), QStringLiteral("在浏览器查看 YDI-D42 语音播报器商品。"),
        QUrl(QStringLiteral("https://e.tb.cn/h.jRnbds15GKTB5Bl?tk=wADl61XJYpV")));
    auto *websiteButton = addLinkButton(QStringLiteral("websiteButton"), QStringLiteral("一点创绘官网"),
        QStringLiteral(":/icons/ydich.png"), QStringLiteral("访问一点创绘官网，查看产品资料与交流内容。"),
        QUrl(QStringLiteral("https://ydich.cn/")));
    header->addLayout(headerLinks);
    header->addStretch();
    byteCount = new QLabel(this);
    byteCount->setObjectName(QStringLiteral("byteCount"));
    byteCount->setProperty("role", "count");
    header->addWidget(byteCount);
    connectionBadge = new QLabel(QStringLiteral("●  未连接"), this);
    connectionBadge->setObjectName(QStringLiteral("connectionBadge"));
    header->addWidget(connectionBadge);
    root->addLayout(header);

    auto *panel = new QFrame(this);
    toolPanel = panel;
    panel->setObjectName(QStringLiteral("toolPanel"));
    auto *controls = new QVBoxLayout(panel);
    controls->setContentsMargins(10, 5, 10, 5);
    controls->setSpacing(6);

    auto *portRow = new QHBoxLayout;
    portRow->setSpacing(8);
    auto *portLabel = new QLabel(QStringLiteral("串口"), panel);
    portLabel->setProperty("role", "fieldLabel");
    portRow->addWidget(portLabel);
    portCombo = new QComboBox(panel);
    portCombo->setObjectName(QStringLiteral("portCombo"));
    portCombo->setAccessibleName(QStringLiteral("串口设备"));
    portLabel->setBuddy(portCombo);
    portCombo->setMinimumWidth(110);
    portCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    portCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    portCombo->setMinimumContentsLength(8);
    connect(portCombo, &QComboBox::currentIndexChanged, this, [this] {
        portCombo->setToolTip(portCombo->currentText());
        if (!portCombo->currentData().toString().isEmpty())
            preferredPort = portCombo->currentData().toString();
        updateControls();
    });
    refreshButton = new AnimatedButton({}, AnimatedButton::Tone::Quiet, panel);
    refreshButton->setObjectName(QStringLiteral("refreshButton"));
    refreshButton->setIcon(UiTheme::refreshIcon());
    refreshButton->setAccessibleName(QStringLiteral("刷新串口列表"));
    refreshButton->setToolTip(QStringLiteral("重新检测可用的串口设备。"));
    refreshButton->setProperty("hintShortcut", QStringLiteral("F5"));
    connect(refreshButton, &QPushButton::clicked, this, [this] {
        updatePorts();
        if (!transaction->isBusy())
            showFeedback(portCombo->currentData().toString().isEmpty()
                    ? QStringLiteral("未发现串口，请检查 USB 连接与驱动。")
                    : QStringLiteral("串口列表已刷新。"));
    });
    toggleButton = new AnimatedButton(QStringLiteral("打开串口"), AnimatedButton::Tone::Success, panel);
    toggleButton->setObjectName(QStringLiteral("toggleButton"));
    toggleButton->setMinimumWidth(86);
    toggleButton->setAccessibleName(QStringLiteral("连接或断开串口"));
    connect(toggleButton, &QPushButton::clicked, this, &SerialSender::toggleSerialPort);
    testButton = new AnimatedButton(QStringLiteral("测试连接"), AnimatedButton::Tone::Warning, this);
    testButton->setObjectName(QStringLiteral("testButton"));
    testButton->setAccessibleName(QStringLiteral("测试连接"));
    testButton->setToolTip(QStringLiteral("检查模块响应，并准备当前声音设置。"));
    connect(testButton, &QPushButton::clicked, this, &SerialSender::testConnection);
    portRow->addWidget(portCombo, 1);
    portRow->addWidget(refreshButton);
    portRow->addWidget(toggleButton);
    controls->addLayout(portRow);

    auto *textRow = new QHBoxLayout;
    textRow->setSpacing(8);
    textInput = new QLineEdit(panel);
    textInput->setObjectName(QStringLiteral("textInput"));
    textInput->setAccessibleName(QStringLiteral("播报文本"));
    textInput->setPlaceholderText(QStringLiteral("请输入要播报的文字…"));
    textInput->setText(QStringLiteral("这是一段测试文本，用于测试语音合成效果。"));
    textInput->setFixedHeight(30);
    connect(textInput, &QLineEdit::textChanged, this, &SerialSender::updateTextState);
    connect(textInput, &QLineEdit::returnPressed, this, &SerialSender::sendData);
    textRow->addWidget(textInput, 1);
    sendButton = new AnimatedButton(QStringLiteral("发送"), AnimatedButton::Tone::Primary, panel);
    sendButton->setObjectName(QStringLiteral("sendButton"));
    sendButton->setMinimumWidth(72);
    sendButton->setAccessibleName(QStringLiteral("发送播报"));
    sendButton->setToolTip(QStringLiteral("将输入的文字发送给模块播报。"));
    sendButton->setProperty("hintShortcut", QStringLiteral("Enter"));
    connect(sendButton, &QPushButton::clicked, this, &SerialSender::sendData);
    textRow->addWidget(sendButton);
    controls->addLayout(textRow);

    volumeSliderWidget = new LevelSliderWidget(panel);
    volumeSliderWidget->setObjectName(QStringLiteral("volumeControl"));
    volumeSliderWidget->setLabelText(QStringLiteral("音量"));
    volumeSliderWidget->setDescription(QStringLiteral("从轻声到洪亮，选择后点击设置"));
    volumeSliderWidget->setButtonTone(AnimatedButton::Tone::Volume);
    connect(volumeSliderWidget, &LevelSliderWidget::applyRequested, this, &SerialSender::applyVolumeLevel);
    controls->addWidget(volumeSliderWidget);
    speedSliderWidget = new LevelSliderWidget(panel);
    speedSliderWidget->setObjectName(QStringLiteral("speedControl"));
    speedSliderWidget->setLabelText(QStringLiteral("语速"));
    speedSliderWidget->setDescription(QStringLiteral("从舒缓到明快，选择后点击设置"));
    speedSliderWidget->setButtonTone(AnimatedButton::Tone::Speed);
    connect(speedSliderWidget, &LevelSliderWidget::applyRequested, this, &SerialSender::applySpeedLevel);
    controls->addWidget(speedSliderWidget);
    root->addWidget(panel);
    auto *actions = new QHBoxLayout;
    actions->setContentsMargins(2, 0, 2, 0);
    actions->setSpacing(12);
    testButton->setFixedWidth(104);
    actions->addWidget(testButton);
    soundButton = new AnimatedButton(QStringLiteral("音色音调"), AnimatedButton::Tone::Volume, this);
    soundButton->setObjectName(QStringLiteral("soundButton"));
    soundButton->setFixedWidth(104);
    soundPopup = new ParameterPopup(this);
    connect(soundPopup, &ParameterPopup::applyRequested, this, &SerialSender::applySound);
    connect(soundButton, &QPushButton::clicked, this, [this] {
        if (!connected || transaction->isBusy())
            return;
        if (soundPopup->isVisible()) {
            soundPopup->hide();
            return;
        }
        soundPopup->openAt(soundButton, currentConfiguration.voiceId, currentConfiguration.pitch);
    });
    actions->addWidget(soundButton);
    readButton = new AnimatedButton(QStringLiteral("读取配置"), AnimatedButton::Tone::Speed, this);
    readButton->setObjectName(QStringLiteral("readButton"));
    readButton->setFixedWidth(104);
    readButton->setAccessibleName(QStringLiteral("读取模块配置"));
    connect(readButton, &QPushButton::clicked, this, &SerialSender::readConfiguration);
    actions->addWidget(readButton);
    closeButton = new AnimatedButton(QStringLiteral("关闭程序"), AnimatedButton::Tone::Danger, this);
    closeButton->setObjectName(QStringLiteral("closeButton"));
    closeButton->setFixedWidth(104);
    closeButton->setToolTip(QStringLiteral("退出工具，并释放当前串口连接。"));
    closeButton->setProperty("hintShortcut", QStringLiteral("Alt+F4"));
    connect(closeButton, &QPushButton::clicked, this, &QWidget::close);
    actions->addWidget(closeButton);
    root->addLayout(actions);

    notice = new QFrame(this);
    notice->setObjectName(QStringLiteral("notice"));
    auto *noticeLayout = new QHBoxLayout(notice);
    noticeLayout->setContentsMargins(7, 2, 9, 2);
    noticeLayout->setSpacing(7);
    indicator = new ActivityIndicator(notice);
    noticeText = new NoticeCarousel(notice);
    noticeText->setObjectName(QStringLiteral("noticeText"));
    noticeText->setWordWrap(false);
    noticeText->setTextFormat(Qt::PlainText);
    noticeText->setAccessibleName(QStringLiteral("操作结果"));
    connect(noticeText, &NoticeCarousel::toneChanged, this, [this](const QString &tone) {
        notice->setProperty("tone", tone);
        noticeText->setProperty("tone", tone);
        indicator->setState(tone);
        repolish(notice);
        repolish(noticeText);
    });
    noticeText->setTips({ QStringLiteral("按回车发送文本 · F5 刷新串口"),
        QStringLiteral("音量、语速选好档位后点击设置"), QStringLiteral("音色音调一起设置 · 读取配置同步模块参数"),
        QStringLiteral("拖动标题移动窗口 · Alt+F4 关闭") });
    noticeLayout->addWidget(indicator, 0, Qt::AlignTop);
    noticeLayout->addWidget(noticeText, 1);
    auto *footerBrand = new QLabel(QStringLiteral("一点创绘 · Mzee"), notice);
    footerBrand->setObjectName(QStringLiteral("footerBrand"));
    footerBrand->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    noticeLayout->addWidget(footerBrand);
    root->addWidget(notice);

    auto *sendShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this);
    connect(sendShortcut, &QShortcut::activated, this, &SerialSender::sendData);
    auto *keypadShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Enter), this);
    connect(keypadShortcut, &QShortcut::activated, this, &SerialSender::sendData);
    auto *refreshShortcut = new QShortcut(QKeySequence(Qt::Key_F5), this);
    connect(refreshShortcut, &QShortcut::activated, refreshButton, &QPushButton::click);
    setTabOrder(bilibiliButton, taobaoButton);
    setTabOrder(taobaoButton, websiteButton);
    setTabOrder(websiteButton, portCombo);
    setTabOrder(portCombo, refreshButton);
    setTabOrder(refreshButton, toggleButton);
    setTabOrder(toggleButton, textInput);
    setTabOrder(textInput, sendButton);
    setTabOrder(speedSliderWidget->findChild<AnimatedButton *>(), testButton);
    setTabOrder(testButton, soundButton);
    setTabOrder(soundButton, readButton);
    setTabOrder(readButton, closeButton);
    setTabOrder(closeButton, noticeText);
    portCombo->setFocus(Qt::OtherFocusReason);
}

void SerialSender::paintEvent(QPaintEvent *)
{
    const qreal ratio = devicePixelRatioF();
    const QSize pixels = (QSizeF(size()) * ratio).toSize();
    if (backgroundCacheSize != pixels || backgroundCache.devicePixelRatioF() != ratio) {
        backgroundCache = background.scaled(pixels, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        backgroundCache.setDevicePixelRatio(ratio);
        // Cache only the background blur. Controls render normally above it, and
        // hover/slider animation frames reuse this image without running a blur.
        frostedBackgroundCache = makeFrostedBackground(backgroundCache, size(), ratio);
        backgroundCacheSize = pixels;
    }
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const QSizeF scaledSize = backgroundCache.deviceIndependentSize();
    QPainterPath outline;
    outline.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
    painter.fillPath(outline, QColor("#deded6"));
    QBrush texture(backgroundCache);
    QTransform placement;
    placement.translate((width() - scaledSize.width()) / 2, (height() - scaledSize.height()) / 2);
    texture.setTransform(placement);
    painter.fillPath(outline, texture);
    painter.fillPath(outline, QColor(253, 252, 248, 112));
    painter.setPen(QPen(QColor(255, 255, 255, 160), 1));
    painter.drawPath(outline);
    if (toolPanel && toolPanel->isVisible()) {
        const QRectF panelBounds = QRectF(toolPanel->geometry()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath shadow;
        shadow.addRoundedRect(panelBounds.translated(0, 1.5), 10, 10);
        painter.fillPath(shadow, QColor(35, 48, 40, 10));
        QPainterPath glass;
        glass.addRoundedRect(panelBounds, 10, 10);
        painter.fillPath(glass, QBrush(frostedBackgroundCache));
    }
}

void SerialSender::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && event->position().y() < 34) {
        dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        dragging = !(windowHandle() && windowHandle()->startSystemMove());
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void SerialSender::mouseMoveEvent(QMouseEvent *event)
{
    if (dragging && event->buttons().testFlag(Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - dragOffset);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void SerialSender::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        dragging = false;
    QWidget::mouseReleaseEvent(event);
}

void SerialSender::updateControls()
{
    const bool busy = transaction->isBusy();
    portCombo->setEnabled(!connected);
    refreshButton->setEnabled(!busy);
    toggleButton->setEnabled(connected || !portCombo->currentData().toString().isEmpty());
    toggleButton->setText(connected ? QStringLiteral("关闭串口") : QStringLiteral("打开串口"));
    toggleButton->setTone(connected ? AnimatedButton::Tone::Danger : AnimatedButton::Tone::Success);
    toggleButton->setAccessibleName(toggleButton->text());
    toggleButton->setToolTip(busy ? QStringLiteral("取消当前操作并断开连接。")
            : connected           ? QStringLiteral("关闭当前串口连接。")
                                  : QStringLiteral("打开所选串口，连接 TTS 模块。"));
    testButton->setEnabled(connected && !busy);
    soundButton->setEnabled(connected && !busy);
    readButton->setEnabled(connected && !busy);
    soundButton->setToolTip(busy ? QStringLiteral("模块正在处理，完成后可调整音色和音调。")
            : connected          ? QStringLiteral("一起调整音色与五档音调。\n当前：%1；音调 %2（第 %3 档）。")
                              .arg(TtsProtocol::voiceIdLabel(currentConfiguration.voiceId))
                              .arg(currentConfiguration.pitch)
                              .arg(TtsProtocol::nearestLevel(currentConfiguration.pitch, TtsProtocol::PitchValues) + 1)
                        : QStringLiteral("打开串口后，可一起调整音色和音调。"));
    readButton->setText(currentOperation == Operation::ReadConfiguration && busy ? QStringLiteral("读取中")
                                                                                 : QStringLiteral("读取配置"));
    readButton->setToolTip(busy ? QStringLiteral("模块正在处理，请稍后读取配置。")
            : connected         ? QStringLiteral("读取模块当前音量、语速、音色和音调，并映射到五档。")
                                : QStringLiteral("打开串口后，可读取模块当前参数。"));
    sendButton->setEnabled(connected && !busy && validText);
    sendButton->setText(
        currentOperation == Operation::Speech && busy ? QStringLiteral("发送中") : QStringLiteral("发送"));
    volumeSliderWidget->setEnabled(!busy);
    speedSliderWidget->setEnabled(!busy);
    volumeSliderWidget->setApplyEnabled(connected && !busy);
    speedSliderWidget->setApplyEnabled(connected && !busy);
    connectionBadge->setText(connected ? QStringLiteral("●  已连接") : QStringLiteral("●  未连接"));
    connectionBadge->setProperty("connected", connected);
    repolish(connectionBadge);
}

void SerialSender::updateTextState()
{
    const auto encoded = TtsProtocol::encodeText(textInput->text());
    validText = encoded.valid();
    const bool empty = textInput->text().trimmed().isEmpty();
    const bool invalid = !validText && !empty;
    byteCount->setText(encoded.bytes.isEmpty() && invalid ? QStringLiteral("不支持字符")
                                                          : QStringLiteral("%1/500 字节").arg(encoded.bytes.size()));
    byteCount->setProperty("invalid", invalid);
    textInput->setProperty("invalid", invalid);
    repolish(byteCount);
    repolish(textInput);
    textInput->setToolTip(invalid ? encoded.error : QStringLiteral("回车或 Ctrl + Enter 发送；最多 500 字节"));
    textInput->setAccessibleDescription(invalid ? encoded.error : QStringLiteral("最多 500 字节，回车发送。"));
    if (invalid && !transaction->isBusy()) {
        showFeedback(encoded.error, QStringLiteral("error"));
        notice->setProperty("validation", true);
    } else if (!invalid && notice->property("validation").toBool()) {
        showFeedback(empty  ? QStringLiteral("请输入要朗读的内容。")
                : connected ? QStringLiteral("内容已就绪，可发送播报。")
                            : QStringLiteral("打开串口即可播报 · 回车发送"));
    }
    updateControls();
}

void SerialSender::showFeedback(const QString &message, const QString &tone)
{
    notice->setProperty("validation", false);
    noticeText->setMessage(message, tone);
    QAccessibleEvent event(noticeText, QAccessible::Alert);
    QAccessible::updateAccessibility(&event);
}
