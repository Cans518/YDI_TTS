#include "support/memoryport.h"
#include "support/urlcapture.h"
#include "ttstests.h"
#include "ui/animatedbutton.h"
#include "ui/customlevelslider.h"
#include "ui/levelsliderwidget.h"
#include "ui/noticecarousel.h"
#include "ui/parameterpopup.h"
#include "ui/uitheme.h"
#include <QCheckBox>
#include <QElapsedTimer>
#include <QFontInfo>
#include <QLabel>
#include <QLineEdit>
#include <QScreen>
#include <QSettings>
#include <QUrl>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

void TtsTests::validationRecovery()
{
    auto *port = new MemoryPort;
    SerialSender window(port, nullptr);
    prepareWindow(window);
    child<AnimatedButton>(window, "toggleButton")->click();
    auto *editor = child<QLineEdit>(window, "textInput");
    auto *result = child<QLabel>(window, "noticeText");
    editor->setText(QString::fromUtf8("🙂"));
    QVERIFY(QMetaObject::invokeMethod(&window, "sendData"));
    editor->setText(QStringLiteral("有效文本"));
    QVERIFY(!result->text().contains(QStringLiteral("不支持")));
    QVERIFY(child<AnimatedButton>(window, "sendButton")->isEnabled());
    editor->setText(QString::fromUtf8("🙂"));
    editor->clear();
    QVERIFY(result->text().contains(QStringLiteral("请输入")));
    QVERIFY(!child<AnimatedButton>(window, "sendButton")->isEnabled());
}

void TtsTests::validationRecoveryAfterBusyOperation()
{
    auto *port = new MemoryPort;
    port->autoReply = true;
    port->replyDelay = 40;
    SerialSender window(port, nullptr);
    prepareWindow(window);
    child<AnimatedButton>(window, "toggleButton")->click();
    auto *send = child<AnimatedButton>(window, "sendButton");
    auto *editor = child<QLineEdit>(window, "textInput");
    auto *result = child<QLabel>(window, "noticeText");
    send->click();
    QVERIFY(window.findChild<SerialTransaction *>()->isBusy());
    editor->setText(QString::fromUtf8("🙂"));
    QTRY_VERIFY(!window.findChild<SerialTransaction *>()->isBusy());
    QVERIFY(result->toolTip().contains(QStringLiteral("不支持")));
    QVERIFY(child<QWidget>(window, "notice")->property("validation").toBool());
    QVERIFY(!send->isEnabled());
    editor->setText(QStringLiteral("继续播报"));
    QVERIFY(send->isEnabled());
    QVERIFY(!result->toolTip().contains(QStringLiteral("不支持")));
}

void TtsTests::persistentHeaderLinks()
{
    UrlCapture urls;
    auto *port = new MemoryPort;
    SerialSender window(port, nullptr);
    prepareWindow(window);
    auto *bilibili = child<AnimatedButton>(window, "bilibiliButton");
    auto *taobao = child<AnimatedButton>(window, "taobaoButton");
    auto *website = child<AnimatedButton>(window, "websiteButton");
    const QUrl videoUrl(QStringLiteral("https://b23.tv/CDQRwvy"));
    const QUrl productUrl(QStringLiteral("https://e.tb.cn/h.jRnbds15GKTB5Bl?tk=wADl61XJYpV"));
    const QUrl websiteUrl(QStringLiteral("https://ydich.cn/"));
    const QPoint originalPosition = window.pos();
    QVERIFY(bilibili->isEnabled());
    QVERIFY(taobao->isEnabled());
    QVERIFY(website->isEnabled());
    QVERIFY(!bilibili->icon().pixmap(20, 20).isNull());
    QVERIFY(!taobao->icon().pixmap(20, 20).isNull());
    QVERIFY(!website->icon().pixmap(20, 20).isNull());
    QTest::mouseClick(bilibili, Qt::LeftButton);
    QTest::mouseClick(taobao, Qt::LeftButton);
    QTest::mouseClick(website, Qt::LeftButton);
    QCOMPARE(urls.opened, (QList<QUrl> { videoUrl, productUrl, websiteUrl }));
    QCOMPARE(window.pos(), originalPosition);
    QVERIFY(port->writes.isEmpty());

    bilibili->setFocus();
    QTest::keyClick(bilibili, Qt::Key_Tab);
    QCOMPARE(QApplication::focusWidget(), taobao);
    QTest::keyClick(taobao, Qt::Key_Tab);
    QCOMPARE(QApplication::focusWidget(), website);
    QTest::keyClick(website, Qt::Key_Tab);
    QCOMPARE(QApplication::focusWidget(), child<QComboBox>(window, "portCombo"));

    child<AnimatedButton>(window, "toggleButton")->click();
    child<AnimatedButton>(window, "sendButton")->click();
    QVERIFY(window.findChild<SerialTransaction *>()->isBusy());
    QVERIFY(bilibili->isEnabled());
    QVERIFY(taobao->isEnabled());
    QVERIFY(website->isEnabled());
    const auto writes = port->writes;
    bilibili->setFocus();
    QTest::keyClick(bilibili, Qt::Key_Space);
    website->setFocus();
    QTest::keyClick(website, Qt::Key_Space);
    QCOMPARE(urls.opened, (QList<QUrl> { videoUrl, productUrl, websiteUrl, videoUrl, websiteUrl }));
    QCOMPARE(port->writes, writes);
    QCOMPARE(window.pos(), originalPosition);

    port->unplug();
    QVERIFY(bilibili->isEnabled());
    QVERIFY(taobao->isEnabled());
    QVERIFY(website->isEnabled());
    QTest::mouseClick(taobao, Qt::LeftButton);
    QTest::mouseClick(website, Qt::LeftButton);
    QCOMPARE(
        urls.opened, (QList<QUrl> { videoUrl, productUrl, websiteUrl, videoUrl, websiteUrl, productUrl, websiteUrl }));
}

void TtsTests::windowWorkflow()
{
    auto *port = new MemoryPort;
    port->autoReply = true;
    SerialSender window(port, nullptr);
    prepareWindow(window);
    auto *toggle = child<AnimatedButton>(window, "toggleButton");
    auto *send = child<AnimatedButton>(window, "sendButton");
    auto *editor = child<QLineEdit>(window, "textInput");
    QVERIFY(!send->isEnabled());
    QVERIFY(capture(window, "01-disconnected"));
    toggle->click();
    QVERIFY(send->isEnabled());
    QVERIFY(!child<QComboBox>(window, "portCombo")->isEnabled());
    port->replyDelay = 60;
    send->click();
    QVERIFY(!send->isEnabled());
    QVERIFY(capture(window, "02-sending"));
    QTRY_VERIFY(send->isEnabled());
    QCOMPARE(port->writes.size(), 3);
    QCOMPARE(port->writes.last(), TtsProtocol::textFrame(TtsProtocol::encodeText(editor->text()).bytes));
    QVERIFY(child<QLabel>(window, "noticeText")->text().contains(QStringLiteral("已接收")));
    auto *volume = child<LevelSliderWidget>(window, "volumeControl");
    volume->setCurrentLevel(3);
    volume->findChild<AnimatedButton *>()->click();
    QTRY_VERIFY(send->isEnabled());
    QCOMPARE(port->writes.last(), TtsProtocol::textFrame("[v8]"));
    QVERIFY(capture(window, "03-connected"));
    editor->setText(QString(251, QChar(0x4e2d)));
    QVERIFY(!send->isEnabled());
    QCOMPARE(child<QLabel>(window, "byteCount")->text(), QStringLiteral("502/500 字节"));
    QVERIFY(capture(window, "04-validation"));
    editor->setText(QStringLiteral("恢复发送"));
    QVERIFY(send->isEnabled());
    port->autoReply = false;
    send->click();
    QVERIFY(!send->isEnabled());
    port->unplug();
    QVERIFY(!port->isOpen());
    QVERIFY(!send->isEnabled());
    QCOMPARE(toggle->text(), QStringLiteral("打开串口"));
    QVERIFY(capture(window, "05-disconnected-error"));
    window.close();
}

void TtsTests::windowTimeoutAndClose()
{
    auto *port = new MemoryPort;
    SerialSender window(port, nullptr);
    prepareWindow(window);
    child<AnimatedButton>(window, "toggleButton")->click();
    child<AnimatedButton>(window, "testButton")->click();
    QTRY_VERIFY_WITH_TIMEOUT(!port->isOpen(), 1600);
    QCOMPARE(port->writes.size(), 1);
    QVERIFY(child<QLabel>(window, "noticeText")->text().contains(QStringLiteral("超时")));
    child<AnimatedButton>(window, "toggleButton")->click();
    child<AnimatedButton>(window, "sendButton")->click();
    window.close();
    QVERIFY(!port->isOpen());
    QVERIFY(!window.findChild<SerialTransaction *>()->isBusy());
}

void TtsTests::windowSizingAndEmptyPorts()
{
    SerialSender window;
    QCOMPARE(window.size(), QSize(500, 250));
    QVERIFY(window.width() * window.height() <= 450 * 280);
    QVERIFY(window.windowFlags().testFlag(Qt::FramelessWindowHint));
    QVERIFY(!window.windowFlags().testFlag(Qt::WindowMinMaxButtonsHint));
    QVERIFY(!QPixmap(QStringLiteral(":/images/background.png")).isNull());
    window.findChild<QTimer *>(QStringLiteral("portMonitor"))->stop();
    auto *combo = child<QComboBox>(window, "portCombo");
    combo->clear();
    combo->addItem(QStringLiteral("未发现串口设备"), QString());
    QVERIFY(!child<AnimatedButton>(window, "toggleButton")->isEnabled());
    window.show();
    QCoreApplication::processEvents();
    QVERIFY(capture(window, "00-tool"));
    qInfo().noquote() << "Input font:" << QFontInfo(child<QLineEdit>(window, "textInput")->font()).family()
                      << "label font:" << QFontInfo(child<QLabel>(window, "noticeText")->font()).family();
#ifdef Q_OS_WIN
    if (qApp->platformName() == QStringLiteral("windows")) {
        const auto style = GetWindowLongPtr(reinterpret_cast<HWND>(window.winId()), GWL_STYLE);
        QVERIFY((style & WS_CAPTION) == 0);
        QVERIFY((style & (WS_MINIMIZEBOX | WS_MAXIMIZEBOX)) == 0);
    }
#endif
    window.resize(window.minimumSize());
    window.show();
    QCoreApplication::processEvents();
    for (const auto *button : window.findChildren<AnimatedButton *>()) {
        if (!button->isVisible())
            continue;
        QVERIFY2(button->width() >= button->sizeHint().width(), qPrintable(button->objectName()));
        const QRect bounds(button->mapTo(&window, QPoint()), button->size());
        QVERIFY(window.rect().contains(bounds));
    }
    for (const char *name : { "textInput", "noticeText", "headline", "footerBrand" }) {
        const auto *control = child<QWidget>(window, name);
        QVERIFY(window.rect().contains(QRect(control->mapTo(&window, QPoint()), control->size())));
    }
    int previousRight = -1;
    for (const char *name :
        { "headline", "bilibiliButton", "taobaoButton", "websiteButton", "byteCount", "connectionBadge" }) {
        const auto *control = child<QWidget>(window, name);
        QVERIFY2(control->width() >= control->sizeHint().width(), name);
        QVERIFY2(control->geometry().left() > previousRight, name);
        previousRight = control->geometry().right();
    }
    QVERIFY(capture(window, "06-compact-top"));
    QVERIFY(window.findChildren<QCheckBox *>().isEmpty());
    QVERIFY(!UiTheme::reducedMotion());
    auto *status = child<QLabel>(window, "noticeText");
    auto *credit = child<QLabel>(window, "footerBrand");
    QCOMPARE(status->parentWidget(), credit->parentWidget());
    QVERIFY(qAbs(status->geometry().center().y() - credit->geometry().center().y()) <= 1);
    QVERIFY(status->geometry().right() < credit->geometry().left());
    QVERIFY(credit->width() >= credit->sizeHint().width());
    const auto *volumeButton = child<LevelSliderWidget>(window, "volumeControl")->findChild<AnimatedButton *>();
    const auto *speedButton = child<LevelSliderWidget>(window, "speedControl")->findChild<AnimatedButton *>();
    QVERIFY(speedButton->mapTo(&window, QPoint()).y()
            - (volumeButton->mapTo(&window, QPoint()).y() + volumeButton->height())
        >= 6);
    QVERIFY(child<AnimatedButton>(window, "closeButton")->geometry().left()
            - child<AnimatedButton>(window, "testButton")->geometry().right()
        >= 16);
    const auto *test = child<AnimatedButton>(window, "testButton");
    const auto *sound = child<AnimatedButton>(window, "soundButton");
    const auto *read = child<AnimatedButton>(window, "readButton");
    const auto *close = child<AnimatedButton>(window, "closeButton");
    QCOMPARE(test->y(), sound->y());
    QCOMPARE(sound->y(), read->y());
    QCOMPARE(read->y(), close->y());
    QVERIFY(test->geometry().right() < sound->x());
    QVERIFY(sound->geometry().right() < read->x());
    QVERIFY(read->geometry().right() < close->x());
    child<QLineEdit>(window, "textInput")->setText(QString::fromUtf8("🙂"));
    QVERIFY(status->toolTip().contains(QStringLiteral("不支持")));
    QVERIFY(status->width() > 200);
    QVERIFY(capture(window, "07-footer-validation"));
    window.resize(700, 440);
    QCoreApplication::processEvents();
    QCOMPARE(window.size(), QSize(500, 250));
    child<QLineEdit>(window, "textInput")->setFocus();
    QVERIFY(capture(window, "08-keyboard-focus"));
}

void TtsTests::framelessDragSendAndClose()
{
    auto *port = new MemoryPort;
    port->autoReply = true;
    SerialSender window(port, nullptr);
    prepareWindow(window);
    if (qApp->platformName() == QStringLiteral("offscreen")) {
        const QPoint origin = window.pos();
        const QPointF local(20, 16);
        const QPointF global = window.mapToGlobal(local.toPoint());
        const QPoint delta(26, 18);
        QMouseEvent press(QEvent::MouseButtonPress, local, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&window, &press);
        QMouseEvent move(
            QEvent::MouseMove, local + delta, global + delta, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&window, &move);
        QCOMPARE(window.pos(), origin + delta);
        QMouseEvent release(
            QEvent::MouseButtonRelease, local, global + delta, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&window, &release);
    }
    const QPoint position = window.pos();
    auto *editor = child<QLineEdit>(window, "textInput");
    QTest::mouseClick(editor, Qt::LeftButton);
    QCOMPARE(window.pos(), position);
    child<AnimatedButton>(window, "toggleButton")->click();
    QTest::keyClick(editor, Qt::Key_Return);
    QTRY_COMPARE(port->writes.size(), 3);
    QTRY_VERIFY(!window.findChild<SerialTransaction *>()->isBusy());
    port->autoReply = false;
    QTest::keyClick(editor, Qt::Key_Return);
    child<AnimatedButton>(window, "closeButton")->click();
    QVERIFY(!window.isVisible());
    QVERIFY(!port->isOpen());
    QVERIFY(!window.findChild<SerialTransaction *>()->isBusy());
}

void TtsTests::soundSettingsWorkflow()
{
    auto *port = new MemoryPort;
    port->autoReply = true;
    SerialSender window(port, nullptr);
    prepareWindow(window);
    auto *sound = child<AnimatedButton>(window, "soundButton");
    auto *read = child<AnimatedButton>(window, "readButton");
    auto *transaction = window.findChild<SerialTransaction *>();
    QVERIFY(!sound->isEnabled());
    QVERIFY(!read->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(&window, "applySound", Q_ARG(int, 51), Q_ARG(int, 4)));
    QVERIFY(port->writes.isEmpty());
    child<AnimatedButton>(window, "toggleButton")->click();

    sound->click();
    auto *popup = child<ParameterPopup>(window, "soundPopup");
    auto *voice = child<QComboBox>(*popup, "voiceChoice");
    auto *pitch = child<CustomLevelSlider>(*popup, "pitchSlider");
    QVERIFY(popup->isVisible());
    QCOMPARE(window.findChildren<ParameterPopup *>().size(), 1);
    QCOMPARE(popup->width(), 320);
    QCOMPARE(voice->count(), 8);
    QCOMPARE(voice->currentIndex(), 6);
    QCOMPARE(pitch->currentLevel(), 3); // Legacy t9 maps to the lower equally close level.
    QCOMPARE(pitch->minimum(), 0);
    QCOMPARE(pitch->maximum(), 4);
    const QStringList expectedTypes { QStringLiteral("女声"), QStringLiteral("男声"), QStringLiteral("男声"),
        QStringLiteral("女声"), QStringLiteral("特殊音色"), QStringLiteral("女童声"), QStringLiteral("男童声"),
        QStringLiteral("男童声") };
    for (int i = 0; i < 8; ++i)
        QCOMPARE(voice->itemText(i),
            QStringLiteral("音色 %1 · %2").arg(i + 1, 2, 10, QLatin1Char('0')).arg(expectedTypes[i]));
    voice->setCurrentIndex(1);
    QTest::keyClick(pitch, Qt::Key_Home);
    QCOMPARE(pitch->currentLevel(), 0);
    QTest::keyClick(pitch, Qt::Key_End);
    QCOMPARE(pitch->currentLevel(), 4);
    QVERIFY(popup->screen()->availableGeometry().contains(popup->geometry()));
    QTest::qWait(180);
    QVERIFY(captureOverlay(window, *popup, "09-sound-panel"));
    QVERIFY(capture(*popup, "15-sound-settings"));
    child<AnimatedButton>(*popup, "applyParameter")->click();
    QVERIFY(!popup->isVisible());
    QVERIFY(!sound->isEnabled());
    QVERIFY(!read->isEnabled());
    QTRY_VERIFY(!transaction->isBusy());
    QCOMPARE(port->writes.size(), 2);
    QCOMPARE(port->writes[0], QByteArray::fromHex("fd000121"));
    QCOMPARE(port->writes[1], TtsProtocol::textFrame("[t10][m51]"));
    QVERIFY(!QSettings().contains(QStringLiteral("sound/voiceIndex")));
    QVERIFY(!QSettings().contains(QStringLiteral("sound/pitchLevel")));

    sound->click();
    QCOMPARE(voice->currentIndex(), 1);
    QCOMPARE(pitch->currentLevel(), 4);
    voice->setCurrentIndex(4);
    pitch->setCurrentLevel(0);
    QTest::keyClick(popup, Qt::Key_Escape);
    QVERIFY(!popup->isVisible());
    QCOMPARE(port->writes.size(), 2);
    sound->click();
    QCOMPARE(voice->currentIndex(), 1);
    QCOMPARE(pitch->currentLevel(), 4);
    voice->setCurrentIndex(4);
    pitch->setCurrentLevel(0);
    auto *cancel = child<AnimatedButton>(*popup, "cancelParameter");
    cancel->setFocus();
    QTest::keyClick(cancel, Qt::Key_Return);
    QVERIFY(!popup->isVisible());
    QCOMPARE(port->writes.size(), 2);

    child<AnimatedButton>(window, "sendButton")->click();
    QTRY_VERIFY(!transaction->isBusy());
    QCOMPARE(port->writes[2], TtsProtocol::textFrame("[t10]"));
    QCOMPARE(port->writes[3], TtsProtocol::textFrame("[m51]"));
    QCOMPARE(port->writes.size(), 5);
    window.close();

    auto *reopenedPort = new MemoryPort;
    reopenedPort->autoReply = true;
    SerialSender reopened(reopenedPort, nullptr);
    prepareWindow(reopened);
    child<AnimatedButton>(reopened, "toggleButton")->click();
    child<AnimatedButton>(reopened, "sendButton")->click();
    QTRY_VERIFY(!reopened.findChild<SerialTransaction *>()->isBusy());
    QCOMPARE(reopenedPort->writes[0], TtsProtocol::textFrame("[t9]"));
    QCOMPARE(reopenedPort->writes[1], TtsProtocol::textFrame("[m56]"));
    QCOMPARE(child<LevelSliderWidget>(reopened, "volumeControl")->currentLevel(), 0);
    QCOMPARE(child<LevelSliderWidget>(reopened, "speedControl")->currentLevel(), 0);
}

void TtsTests::readConfigurationWorkflow()
{
    auto *port = new MemoryPort;
    port->autoReply = true;
    port->configurationReply = configurationReply(8, 17, 9, 57);
    SerialSender window(port, nullptr);
    prepareWindow(window);
    auto *read = child<AnimatedButton>(window, "readButton");
    auto *sound = child<AnimatedButton>(window, "soundButton");
    auto *transaction = window.findChild<SerialTransaction *>();
    auto *volume = child<LevelSliderWidget>(window, "volumeControl");
    auto *speed = child<LevelSliderWidget>(window, "speedControl");
    QVERIFY(!read->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(&window, "readConfiguration"));
    QVERIFY(port->writes.isEmpty());
    child<AnimatedButton>(window, "toggleButton")->click();
    read->click();
    QVERIFY(!read->isEnabled());
    QVERIFY(!sound->isEnabled());
    QCOMPARE(port->writes, (QList<QByteArray> { QByteArray::fromHex("fd000105") }));
    QTRY_VERIFY(!transaction->isBusy());
    QCOMPARE(volume->currentLevel(), 3);
    QCOMPARE(speed->currentLevel(), 2);
    QCOMPARE(volume->findChild<AnimatedButton *>()->text(), QStringLiteral("已读取"));
    QCOMPARE(speed->findChild<AnimatedButton *>()->text(), QStringLiteral("已读取"));
    QVERIFY(child<QLabel>(window, "noticeText")->toolTip().contains(QStringLiteral("语速 17")));
    QTest::qWait(180);
    QVERIFY(capture(window, "10-read-configuration"));
    sound->click();
    auto *popup = child<ParameterPopup>(window, "soundPopup");
    auto *voice = child<QComboBox>(*popup, "voiceChoice");
    auto *pitch = child<CustomLevelSlider>(*popup, "pitchSlider");
    QCOMPARE(voice->currentData().toInt(), 57);
    QCOMPARE(pitch->currentLevel(), 3);
    QVERIFY(capture(*popup, "16-read-sound-settings"));
    child<AnimatedButton>(*popup, "cancelParameter")->click();
    QCOMPARE(port->writes.size(), 1); // Reading and opening the panel do not write settings.
    child<AnimatedButton>(window, "sendButton")->click();
    QTRY_VERIFY(!transaction->isBusy());
    QCOMPARE(port->writes[1], TtsProtocol::textFrame("[t9]")); // Preserve the raw read value until explicitly set.
    QCOMPARE(port->writes[2], TtsProtocol::textFrame("[m57]"));

    port->configurationReply = configurationReply(0, 1, 10, 79);
    read->click();
    QTRY_VERIFY(!transaction->isBusy());
    QCOMPARE(volume->currentLevel(), 0);
    QCOMPARE(speed->currentLevel(), 0);
    sound->click();
    QCOMPARE(voice->currentData().toInt(), 79);
    QCOMPARE(voice->count(), 9);
    QCOMPARE(pitch->currentLevel(), 4);
    child<AnimatedButton>(*popup, "cancelParameter")->click();

    volume->setCurrentLevel(2);
    speed->setCurrentLevel(3);
    port->configurationReply = configurationReply(11, 30, 2, 3);
    read->click();
    QTRY_VERIFY(!transaction->isBusy());
    QVERIFY(!port->isOpen());
    QCOMPARE(volume->currentLevel(), 2);
    QCOMPARE(speed->currentLevel(), 3);
    QVERIFY(!read->isEnabled());
    const int writesBeforeReconnect = port->writes.size();
    child<AnimatedButton>(window, "toggleButton")->click();
    child<AnimatedButton>(window, "sendButton")->click();
    QTRY_VERIFY(!transaction->isBusy());
    QCOMPARE(port->writes[writesBeforeReconnect], TtsProtocol::textFrame("[t10]"));
    QCOMPARE(port->writes[writesBeforeReconnect + 1], TtsProtocol::textFrame("[m79]"));
}

void TtsTests::startupIgnoresSavedPreferences()
{
    QSettings().setValue(QStringLiteral("connection/port"), QStringLiteral("COM99"));
    QSettings().setValue(QStringLiteral("sound/voiceIndex"), 1);
    QSettings().setValue(QStringLiteral("sound/pitchLevel"), 2);
    auto *port = new MemoryPort;
    port->autoReply = true;
    SerialSender window(port, nullptr);
    prepareWindow(window);
    child<AnimatedButton>(window, "toggleButton")->click();
    child<AnimatedButton>(window, "sendButton")->click();
    QTRY_VERIFY(!window.findChild<SerialTransaction *>()->isBusy());
    QCOMPARE(port->writes[0], TtsProtocol::textFrame("[t9]"));
    QCOMPARE(port->writes[1], TtsProtocol::textFrame("[m56]"));
    QCOMPARE(QSettings().value(QStringLiteral("connection/port")).toString(), QStringLiteral("COM99"));
    QCOMPARE(QSettings().value(QStringLiteral("sound/voiceIndex")).toInt(), 1);
    QCOMPARE(QSettings().value(QStringLiteral("sound/pitchLevel")).toInt(), 2);
}

void TtsTests::failedSoundChangeKeepsConfirmedSettings()
{
    auto *port = new MemoryPort;
    port->autoReply = true;
    SerialSender window(port, nullptr);
    prepareWindow(window);
    child<AnimatedButton>(window, "toggleButton")->click();
    auto *transaction = window.findChild<SerialTransaction *>();
    QVERIFY(QMetaObject::invokeMethod(&window, "applySound", Q_ARG(int, 51), Q_ARG(int, 2)));
    QTRY_VERIFY(!transaction->isBusy());
    child<AnimatedButton>(window, "soundButton")->click();
    auto *popup = child<ParameterPopup>(window, "soundPopup");
    child<QComboBox>(*popup, "voiceChoice")->setCurrentIndex(4);
    child<CustomLevelSlider>(*popup, "pitchSlider")->setCurrentLevel(0);
    port->autoReply = false;
    child<AnimatedButton>(*popup, "applyParameter")->click();
    port->rejectWrites = true;
    port->feed("\x4f"); // Let the status query pass, then fail the combined parameter write.
    QTRY_VERIFY(!port->isOpen());
    QVERIFY(!popup->isVisible());
    QVERIFY(!child<AnimatedButton>(window, "soundButton")->isEnabled());
    const int writesBeforeReconnect = port->writes.size();
    port->rejectWrites = false;
    port->autoReply = true;
    child<AnimatedButton>(window, "toggleButton")->click();
    child<AnimatedButton>(window, "sendButton")->click();
    QTRY_VERIFY(!transaction->isBusy());
    QCOMPARE(port->writes[writesBeforeReconnect], TtsProtocol::textFrame("[t6]"));
    QCOMPARE(port->writes[writesBeforeReconnect + 1], TtsProtocol::textFrame("[m51]"));
}

void TtsTests::noticeCarouselIntegration()
{
    auto *port = new MemoryPort;
    port->autoReply = true;
    SerialSender window(port, nullptr);
    prepareWindow(window);
    auto *carousel = child<NoticeCarousel>(window, "noticeText");
    carousel->setRotationInterval(60000);
    auto *notice = child<QWidget>(window, "notice");
    auto *credit = child<QLabel>(window, "footerBrand");
    const QRect creditGeometry = credit->geometry();
    child<AnimatedButton>(window, "toggleButton")->click();
    QVERIFY(capture(window, "12-carousel-current"));
    QCOMPARE(notice->property("tone").toString(), QStringLiteral("success"));
    QElapsedTimer transitionTime;
    transitionTime.start();
    carousel->next();
    QCoreApplication::processEvents();
    auto *transition = carousel->findChild<QVariantAnimation *>();
    QVERIFY(transition);
    if (transitionTime.elapsed() < transition->duration())
        QCOMPARE(transition->state(), QAbstractAnimation::Running);
    QCOMPARE(notice->property("tone").toString(), QStringLiteral("info"));
    QTest::qWait(230);
    QVERIFY(capture(window, "13-carousel-next"));
    QCOMPARE(credit->geometry(), creditGeometry);

    auto *editor = child<QLineEdit>(window, "textInput");
    editor->setText(QString::fromUtf8("🙂"));
    QVERIFY(notice->property("validation").toBool());
    carousel->next();
    QVERIFY(notice->property("validation").toBool());
    QCOMPARE(notice->property("tone").toString(), QStringLiteral("error"));
    editor->setText(QStringLiteral("内容已修正"));
    QVERIFY(!notice->property("validation").toBool());
    QVERIFY(carousel->text().contains(QStringLiteral("已就绪")));

    carousel->setMessage(QStringLiteral("串口连接已中断，请检查模块供电、USB 数据线和所选端口，重新连接设备后再试。"),
        QStringLiteral("error"));
    QVERIFY(carousel->pageCount() > 1);
    QVERIFY(capture(window, "14-carousel-long-error"));
    QCOMPARE(credit->geometry(), creditGeometry);
    QCOMPARE(window.size(), QSize(500, 250));
}
