#include <QAccessible>
#include <QAccessibleValueInterface>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
#include <QEnterEvent>
#include <QFontDatabase>
#include <QFontInfo>
#include <QHelpEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QRawFont>
#include <QScreen>
#include <QSerialPort>
#include <QSettings>
#include <QTemporaryDir>
#include <QUrl>
#include <QVBoxLayout>
#include <QtTest>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
#include <cstring>

#include "app/serialsender.h"
#include "core/serialtransaction.h"
#include "core/ttsprotocol.h"
#include "ui/activityindicator.h"
#include "ui/animatedbutton.h"
#include "ui/customlevelslider.h"
#include "ui/levelsliderwidget.h"
#include "ui/noticecarousel.h"
#include "ui/parameterpopup.h"
#include "ui/uitheme.h"

class MemoryPort : public QSerialPort
{
public:
    QList<QByteArray> writes;
    QByteArray incoming;
    QByteArray configurationReply;
    bool rejectWrites = false;
    bool autoReply = false;
    int replyDelay = 5;

    ~MemoryPort() override
    {
        if (isOpen())
            close();
    }
    bool open(OpenMode mode) override { return QIODevice::open(mode); }
    void close() override
    {
        QIODevice::close();
        incoming.clear();
    }
    qint64 bytesAvailable() const override { return incoming.size() + QIODevice::bytesAvailable(); }
    void feed(const QByteArray &bytes)
    {
        incoming += bytes;
        emit readyRead();
    }
    void unplug() { emit errorOccurred(QSerialPort::ResourceError); }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        const qint64 size = qMin(maxSize, qint64(incoming.size()));
        std::memcpy(data, incoming.constData(), size_t(size));
        incoming.remove(0, size);
        return size;
    }
    qint64 writeData(const char *data, qint64 size) override
    {
        if (rejectWrites) {
            setErrorString(QStringLiteral("模拟写入失败"));
            return -1;
        }
        writes.append(QByteArray(data, size));
        const QByteArray reply
            = QByteArray(data, size) == TtsProtocol::readConfigurationFrame() ? configurationReply : QByteArray("\x4f");
        if (autoReply)
            QTimer::singleShot(replyDelay, this, [this, reply] {
                if (isOpen())
                    feed(reply);
            });
        return size;
    }
};

// Capture real desktop-service requests without launching a browser during tests.
class UrlCapture : public QObject
{
    Q_OBJECT
public:
    QList<QUrl> opened;
    UrlCapture()
    {
        QDesktopServices::setUrlHandler(QStringLiteral("https"), this, "record");
        QDesktopServices::setUrlHandler(QStringLiteral("http"), this, "record");
    }
    ~UrlCapture() override
    {
        QDesktopServices::unsetUrlHandler(QStringLiteral("https"));
        QDesktopServices::unsetUrlHandler(QStringLiteral("http"));
    }
public slots:
    void record(const QUrl &url) { opened.append(url); }
};

class TtsTests : public QObject
{
    Q_OBJECT
private:
    QTemporaryDir settings;

    static QByteArray configurationReply(int volume, int speed, int pitch, int voice)
    {
        // Fixed configuration-response example, with four values varied.
        auto reply = QByteArray::fromHex("5f a8 03 05 08 05 03 01 01 02 00 00 00 00 00 00 00 01");
        reply[3] = char(speed);
        reply[4] = char(volume);
        reply[5] = char(pitch);
        reply[6] = char(voice);
        return reply;
    }

    template <class T> T *child(QWidget &window, const char *name)
    {
        auto *widget = window.findChild<T *>(QString::fromLatin1(name));
        Q_ASSERT(widget);
        return widget;
    }

    void prepareWindow(SerialSender &window)
    {
        window.findChild<QTimer *>(QStringLiteral("portMonitor"))->stop();
        auto *combo = child<QComboBox>(window, "portCombo");
        combo->clear();
        combo->addItem(QStringLiteral("COM7 · 长风 TTS 测试设备"), QStringLiteral("COM7"));
        window.show();
        QCoreApplication::processEvents();
    }

    bool capture(QWidget &window, const QString &name)
    {
        const QString directory = qEnvironmentVariable("YDI_UI_CAPTURE_DIR");
        if (directory.isEmpty())
            return true;
        QDir().mkpath(directory);
        QCoreApplication::processEvents();
        return window.grab().save(QDir(directory).filePath(name + QStringLiteral(".png")));
    }

    bool captureOverlay(QWidget &window, QWidget &overlay, const QString &name)
    {
        const QString directory = qEnvironmentVariable("YDI_UI_CAPTURE_DIR");
        if (directory.isEmpty())
            return true;
        QCoreApplication::processEvents();
        const QRect bounds = window.geometry().united(overlay.geometry()).adjusted(-4, -4, 4, 4);
        const qreal ratio = window.devicePixelRatioF();
        QPixmap result((QSizeF(bounds.size()) * ratio).toSize());
        result.setDevicePixelRatio(ratio);
        result.fill(QColor("#edf1ec"));
        QPainter painter(&result);
        painter.drawPixmap(window.pos() - bounds.topLeft(), window.grab());
        painter.drawPixmap(overlay.pos() - bounds.topLeft(), overlay.grab());
        painter.end();
        return result.save(QDir(directory).filePath(name + QStringLiteral(".png")));
    }

    QWidget *visibleHint() const
    {
        for (auto *widget : QApplication::topLevelWidgets())
            if (widget->objectName() == QStringLiteral("hintBubble") && widget->isVisible())
                return widget;
        return nullptr;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(settings.isValid());
        QCoreApplication::setOrganizationName(QStringLiteral("YDI-Tests"));
        QCoreApplication::setApplicationName(QStringLiteral("TTS-Tests"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        QSettings().setValue(QStringLiteral("appearance/reducedMotion"), true);
        UiTheme::install(*qApp);
        QVERIFY(!UiTheme::reducedMotion()); // A retired setting must not turn animations off.
        const QRawFont suppliedFont(QStringLiteral(":/fonts/LXGWWenKaiMonoGBScreen.ttf"), 13);
        QVERIFY(suppliedFont.isValid());
        QCOMPARE(QFontInfo(qApp->font()).family(), suppliedFont.familyName());
        qInfo().noquote() << "UI font:" << QFontInfo(qApp->font()).family()
                          << "requested:" << qApp->font().families().join(", ");
    }

    void cleanup()
    {
        UiTheme::setReducedMotion(false);
        QSettings().clear();
    }

    void protocolLength_data()
    {
        QTest::addColumn<int>("size");
        QTest::newRow("small") << 1;
        QTest::newRow("last-low-byte") << 253;
        QTest::newRow("first-high-byte") << 254;
        QTest::newRow("maximum") << 500;
    }

    void protocolLength()
    {
        QFETCH(int, size);
        const QByteArray payload(size, 'a');
        const auto frame = TtsProtocol::textFrame(payload);
        QCOMPARE(frame.size(), size + 5);
        QCOMPARE(quint8(frame[0]), quint8(0xfd));
        QCOMPARE((quint8(frame[1]) << 8) | quint8(frame[2]), size + 2);
        QCOMPARE(frame.mid(3, 2), QByteArray::fromHex("0101"));
        QCOMPARE(frame.mid(5), payload);
    }

    void encodingAndLimits()
    {
        const auto chinese = TtsProtocol::encodeText(QStringLiteral("中文ABC"));
        QVERIFY2(chinese.valid(), qPrintable(chinese.error));
        QCOMPARE(chinese.bytes, QByteArray::fromHex("d6d0cec4414243"));
        QVERIFY(!TtsProtocol::encodeText(QString::fromUtf8("🙂")).valid());
        QVERIFY(!TtsProtocol::encodeText(QStringLiteral(" \n\t")).valid());
        QVERIFY(TtsProtocol::encodeText(QString(250, QChar(0x4e2d))).valid());
        QVERIFY(!TtsProtocol::encodeText(QString(251, QChar(0x4e2d))).valid());
        QVERIFY(TtsProtocol::textFrame(QByteArray(501, 'x')).isEmpty());
        QVERIFY(TtsProtocol::textFrame({}).isEmpty());
        QCOMPARE(TtsProtocol::textFrame("[t9]"), QByteArray::fromHex("fd000601015b74395d"));
        QCOMPARE(TtsProtocol::textFrame("[m56]"), QByteArray::fromHex("fd000701015b6d35365d"));
        for (int level = 0; level < 5; ++level) {
            QCOMPARE(TtsProtocol::volumeCommand(level), "[v" + QByteArray::number((level + 1) * 2) + ']');
            QCOMPARE(TtsProtocol::speedCommand(level), "[s" + QByteArray::number((level + 1) * 6) + ']');
        }
    }

    void voiceAndPitchProtocol()
    {
        const QList<QByteArray> voices { "[m3]", "[m51]", "[m52]", "[m53]", "[m54]", "[m55]", "[m56]", "[m57]" };
        const QList<QByteArray> pitches { "[t2]", "[t4]", "[t6]", "[t8]", "[t10]" };
        for (int index = 0; index < voices.size(); ++index)
            QCOMPARE(TtsProtocol::voiceCommand(index), voices[index]);
        for (int level = 0; level < pitches.size(); ++level)
            QCOMPARE(TtsProtocol::pitchCommand(level), pitches[level]);
        QCOMPARE(TtsProtocol::textFrame(TtsProtocol::voiceCommand(1)), QByteArray::fromHex("fd000701015b6d35315d"));
        QCOMPARE(TtsProtocol::textFrame(TtsProtocol::pitchCommand(4)), QByteArray::fromHex("fd000701015b7431305d"));
        QVERIFY(TtsProtocol::voiceCommand(-1).isEmpty());
        QVERIFY(TtsProtocol::voiceCommand(8).isEmpty());
        QVERIFY(TtsProtocol::pitchCommand(-1).isEmpty());
        QVERIFY(TtsProtocol::pitchCommand(5).isEmpty());
    }

    void configurationProtocolAndMapping()
    {
        const auto documentedReply = QByteArray::fromHex("5f a8 03 05 08 05 03 01 01 02 00 00 00 00 00 00 00 01");
        const auto parsed = TtsProtocol::parseConfiguration(documentedReply);
        QVERIFY(parsed);
        QCOMPARE(parsed->speed, 5);
        QCOMPARE(parsed->volume, 8);
        QCOMPARE(parsed->pitch, 5);
        QCOMPARE(parsed->voiceId, 3);
        QCOMPARE(TtsProtocol::readConfigurationFrame(), QByteArray::fromHex("fd000105"));
        QVERIFY(!TtsProtocol::parseConfiguration(documentedReply.left(17)));
        QVERIFY(!TtsProtocol::parseConfiguration(documentedReply + '\0'));
        QVERIFY(!TtsProtocol::parseConfiguration(configurationReply(11, 5, 5, 3)));
        QVERIFY(!TtsProtocol::parseConfiguration(configurationReply(8, 0, 5, 3)));
        QVERIFY(!TtsProtocol::parseConfiguration(configurationReply(8, 31, 5, 3)));
        QVERIFY(!TtsProtocol::parseConfiguration(configurationReply(8, 5, 0, 3)));
        QVERIFY(!TtsProtocol::parseConfiguration(configurationReply(8, 5, 11, 3)));
        QVERIFY(TtsProtocol::parseConfiguration(configurationReply(0, 1, 1, 79)));
        QCOMPARE(TtsProtocol::nearestLevel(0, TtsProtocol::VolumeValues), 0);
        QCOMPARE(TtsProtocol::nearestLevel(3, TtsProtocol::VolumeValues), 0);
        QCOMPARE(TtsProtocol::nearestLevel(8, TtsProtocol::VolumeValues), 3);
        QCOMPARE(TtsProtocol::nearestLevel(17, TtsProtocol::SpeedValues), 2);
        QCOMPARE(TtsProtocol::nearestLevel(30, TtsProtocol::SpeedValues), 4);
        QCOMPARE(TtsProtocol::nearestLevel(9, TtsProtocol::PitchValues), 3);
        QCOMPARE(TtsProtocol::pitchValueCommand(9), QByteArray("[t9]"));
        QCOMPARE(TtsProtocol::voiceIdCommand(79), QByteArray("[m79]"));
    }

    void configurationRepliesAreFramed()
    {
        const SerialTransaction::Step read { TtsProtocol::readConfigurationFrame(), QStringLiteral("读取配置"),
            SerialTransaction::Reply::Configuration };
        auto reply = configurationReply(8, 17, 9, 79);
        reply[10] = char(0x45);
        reply[11] = char(0x5f);
        for (int split = 1; split < reply.size(); ++split) {
            MemoryPort port;
            QVERIFY(port.open(QIODevice::ReadWrite));
            SerialTransaction transaction(&port);
            QSignalSpy finished(&transaction, &SerialTransaction::finished);
            QVERIFY(transaction.start({ read }));
            port.feed(QByteArray::fromHex("414f52544e") + reply.left(split));
            QVERIFY(transaction.isBusy());
            QVERIFY(transaction.response().isEmpty());
            QCOMPARE(finished.count(), 0);
            port.feed(reply.mid(split) + reply + QByteArray("\x4f"));
            QTRY_COMPARE(finished.count(), 1);
            QVERIFY(finished.first()[0].toBool());
            QCOMPARE(transaction.response(), reply);
            QCOMPARE(port.writes.size(), 1);
        }

        MemoryPort port;
        QVERIFY(port.open(QIODevice::ReadWrite));
        SerialTransaction transaction(&port);
        QSignalSpy finished(&transaction, &SerialTransaction::finished);
        QVERIFY(transaction.start({ { "set", "设置" } }));
        port.feed(reply); // 4F in the voice field is data, not acknowledgement.
        port.feed(QByteArray("\x5e") + QByteArray(12, char(0x4f)));
        QTest::qWait(50);
        QCOMPARE(finished.count(), 0);
        port.feed("\x4f");
        QTRY_COMPARE(finished.count(), 1);

        finished.clear();
        QVERIFY(transaction.start({ read }, 30));
        port.feed("\x4f"); // An ACK alone cannot complete a configuration read.
        port.feed(reply.left(10));
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(!finished.first()[0].toBool());
        QVERIFY(transaction.response().isEmpty());

        finished.clear();
        QVERIFY(transaction.start({ read }));
        port.feed(configurationReply(11, 5, 5, 3));
        QCOMPARE(finished.count(), 1);
        QVERIFY(!finished.first()[0].toBool());
        QVERIFY(transaction.response().isEmpty());

        finished.clear();
        const int requestsBeforeReject = port.writes.size();
        QVERIFY(transaction.start({ read }));
        QTRY_COMPARE(port.writes.size(), requestsBeforeReject + 1);
        port.feed("\x45");
        QCOMPARE(finished.count(), 1);
        QVERIFY(!finished.first()[0].toBool());
    }

    void asynchronousSequence()
    {
        MemoryPort port;
        QVERIFY(port.open(QIODevice::ReadWrite));
        SerialTransaction transaction(&port);
        QSignalSpy finished(&transaction, &SerialTransaction::finished);
        const QVector<SerialTransaction::Step> steps { { "first", "one" }, { "second", "two" } };
        QVERIFY(transaction.start(steps));
        QCOMPARE(port.writes, QList<QByteArray> { "first" });
        QVERIFY(!transaction.start(steps));
        bool heartbeat = false;
        QTimer::singleShot(0, [&heartbeat] { heartbeat = true; });
        QTRY_VERIFY(heartbeat); // Waiting for the module must leave the event loop free.
        port.feed("noise");
        QCOMPARE(port.writes.size(), 1);
        QElapsedTimer frameGap;
        frameGap.start();
        port.feed("\x4f\x4f"); // One batch cannot acknowledge two distinct commands.
        QTRY_COMPARE(port.writes.size(), 2);
        QVERIFY(frameGap.elapsed() >= 30);
        QCOMPARE(finished.count(), 0);
        port.feed("\x4f");
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(finished.first()[0].toBool());
        QVERIFY(!transaction.isBusy());
    }

    void timeoutAndWriteFailure()
    {
        MemoryPort port;
        QVERIFY(port.open(QIODevice::ReadWrite));
        SerialTransaction transaction(&port);
        QSignalSpy finished(&transaction, &SerialTransaction::finished);
        QVERIFY(transaction.start({ { "init", "初始化" }, { "speech", "发送" } }, 20));
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(!finished.first()[0].toBool());
        QCOMPARE(port.writes.size(), 1); // Never send speech after failed initialization.
        port.feed("\x4f");
        QCoreApplication::processEvents();
        QCOMPARE(port.writes.size(), 1);
        port.rejectWrites = true;
        QVERIFY(transaction.start({ { "retry", "重试" } }));
        QTRY_COMPARE(finished.count(), 2);
        QVERIFY(!transaction.isBusy());
    }

    void cancelAndRestart()
    {
        MemoryPort port;
        QVERIFY(port.open(QIODevice::ReadWrite));
        SerialTransaction transaction(&port);
        QSignalSpy finished(&transaction, &SerialTransaction::finished);
        QVERIFY(transaction.start({ { "old", "旧命令" }, { "must-not-send", "旧后续" } }));
        QElapsedTimer gapAfterAcknowledgement;
        gapAfterAcknowledgement.start();
        port.feed("\x4f"); // Queues progression, but cancellation invalidates that callback.
        transaction.cancel();
        QVERIFY(transaction.start({ { "new", "新命令" } }));
        QTRY_COMPARE(port.writes, (QList<QByteArray> { "old", "new" }));
        QVERIFY(gapAfterAcknowledgement.elapsed() >= 30);
        QCOMPARE(finished.count(), 1);
        port.close();
        QCOMPARE(finished.count(), 2);
        QVERIFY(!transaction.isBusy());
    }

    void reentrantProgressCancellation()
    {
        MemoryPort port;
        QVERIFY(port.open(QIODevice::ReadWrite));
        SerialTransaction transaction(&port);
        connect(&transaction, &SerialTransaction::progress, &transaction, [&](const QString &label) {
            if (label == "old") {
                transaction.cancel();
                transaction.start({ { "new-frame", "new" } });
            }
        });
        QVERIFY(transaction.start({ { "old-frame", "old" } }));
        QCOMPARE(port.writes, QList<QByteArray> { "new-frame" });
        transaction.cancel();
    }

    void sliderBoundariesAndKeyboard()
    {
        CustomLevelSlider slider;
        slider.resize(280, 44);
        slider.setAccessibleName(QStringLiteral("音量"));
        slider.show();
        slider.setFocus();
        QSignalSpy changed(&slider, &CustomLevelSlider::levelChanged);
        QTest::keyClick(&slider, Qt::Key_End);
        QCOMPARE(slider.currentLevel(), 4);
        QCOMPARE(changed.count(), 1);
        QTest::keyClick(&slider, Qt::Key_Left);
        QCOMPARE(slider.currentLevel(), 3);
        slider.setLevels(1);
        QCOMPARE(slider.currentLevel(), 0);
        slider.resize(20, 44);
        QTest::mouseClick(&slider, Qt::LeftButton, Qt::NoModifier, QPoint(10, 20));
        QCOMPARE(slider.currentLevel(), 0);
        QVERIFY(!slider.grab().isNull());
        slider.setLevels(5);
        slider.resize(280, 44);
        UiTheme::setReducedMotion(true);
        slider.setCurrentLevel(4);
        QCoreApplication::processEvents();
        const QImage atEnd = slider.grab().toImage();
        QTest::qWait(30);
        QCOMPARE(slider.grab().toImage(), atEnd);
        auto *accessible = QAccessible::queryAccessibleInterface(&slider);
        QVERIFY(accessible);
        QCOMPARE(accessible->role(), QAccessible::Slider);
        QVERIFY(accessible->valueInterface());
        QCOMPARE(accessible->valueInterface()->currentValue().toInt(), 4);
    }

    void sliderAnimationLifecycle()
    {
        CustomLevelSlider slider;
        slider.resize(300, 44);
        slider.show();
        QCoreApplication::processEvents();
        const QImage initial = slider.grab().toImage();
        slider.setCurrentLevel(4);
        QCOMPARE(slider.currentLevel(), 4); // Value changes before the visual transition finishes.
        const QImage starting = slider.grab().toImage();
        QTest::qWait(220);
        const QImage settled = slider.grab().toImage();
        QVERIFY(settled != initial);
        QVERIFY(settled != starting);
        slider.setCurrentLevel(0);
        slider.hide();
        slider.show();
        const QImage afterShow = slider.grab().toImage();
        QTest::qWait(200);
        QCOMPARE(slider.grab().toImage(), afterShow);
    }

    void buttonAndActivityMotion()
    {
        AnimatedButton button(QStringLiteral("发送"));
        button.resize(90, 34);
        button.show();
        QCoreApplication::processEvents();
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(&button, &leave);
        QTest::qWait(180);
        const QImage normal = button.grab().toImage();
        QEnterEvent enter(QPointF(8, 8), QPointF(8, 8), QPointF(8, 8));
        QApplication::sendEvent(&button, &enter);
        QTest::qWait(180);
        QVERIFY(button.grab().toImage() != normal);
        UiTheme::setReducedMotion(true);
        QApplication::sendEvent(&button, &leave);
        QCOMPARE(button.grab().toImage(), normal);

        UiTheme::setReducedMotion(false);
        ActivityIndicator activity;
        activity.show();
        activity.activateWindow();
        QCoreApplication::processEvents();
        auto *ticker = activity.findChild<QTimer *>();
        QVERIFY(ticker);
        activity.setState(QStringLiteral("busy"));
        QCOMPARE(ticker->isActive(), qApp->applicationState() == Qt::ApplicationActive);
        activity.hide();
        QVERIFY(!ticker->isActive());
        activity.show();
        UiTheme::setReducedMotion(true);
        QVERIFY(!ticker->isActive());
        UiTheme::setReducedMotion(false);
        activity.setState(QStringLiteral("success"));
        QVERIFY(!ticker->isActive());
    }

    void validationRecovery()
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

    void validationRecoveryAfterBusyOperation()
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

    void persistentHeaderLinks()
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
        QCOMPARE(urls.opened,
            (QList<QUrl> { videoUrl, productUrl, websiteUrl, videoUrl, websiteUrl, productUrl, websiteUrl }));
    }

    void windowWorkflow()
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

    void windowTimeoutAndClose()
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

    void windowSizingAndEmptyPorts()
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

    void framelessDragSendAndClose()
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

    void soundSettingsWorkflow()
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

    void readConfigurationWorkflow()
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

    void startupIgnoresSavedPreferences()
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

    void failedSoundChangeKeepsConfirmedSettings()
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

    void styledHoverHints()
    {
        SerialSender window;
        window.findChild<QTimer *>(QStringLiteral("portMonitor"))->stop();
        window.show();
        QCoreApplication::processEvents();
        auto *button = child<AnimatedButton>(window, "refreshButton");
        const QPoint local = button->rect().center();
        QHelpEvent help(QEvent::ToolTip, local, button->mapToGlobal(local));
        QApplication::sendEvent(button, &help);
        QTRY_VERIFY(visibleHint());
        auto *hint = visibleHint();
        QCOMPARE(hint->focusPolicy(), Qt::NoFocus);
        QCOMPARE(hint->findChild<QLabel *>(QStringLiteral("hintShortcut"))->text(), QStringLiteral("F5"));
        QCOMPARE(hint->findChild<QLabel *>(QStringLiteral("hintBody"))->text(), button->toolTip());
        QTest::qWait(160);
        QVERIFY(captureOverlay(window, *hint, "11-hover-hint"));
        QVERIFY(capture(*hint, "17-hover-hint-detail"));
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(button, &leave);
        QVERIFY(!visibleHint());

        QHelpEvent cornerHelp(
            QEvent::ToolTip, local, button->screen()->availableGeometry().bottomRight() - QPoint(2, 2));
        QApplication::sendEvent(button, &cornerHelp);
        QTRY_VERIFY(visibleHint());
        QVERIFY(visibleHint()->screen()->availableGeometry().contains(visibleHint()->geometry()));
        button->setToolTip(QStringLiteral("新的提示文字"));
        QVERIFY(!visibleHint());
        QApplication::sendEvent(button, &help);
        QTRY_VERIFY(visibleHint());
        QTest::mouseClick(button, Qt::LeftButton);
        QVERIFY(!visibleHint());
        QApplication::sendEvent(button, &help);
        QTRY_VERIFY(visibleHint());
        window.close();
        QVERIFY(!visibleHint());
        window.show();
        auto *temporaryButton
            = new AnimatedButton(QStringLiteral("临时按钮"), AnimatedButton::Tone::Secondary, &window);
        temporaryButton->setToolTip(QStringLiteral("控件销毁后应立即收起"));
        temporaryButton->show();
        QHelpEvent temporaryHelp(QEvent::ToolTip, QPoint(5, 5), temporaryButton->mapToGlobal(QPoint(5, 5)));
        QApplication::sendEvent(temporaryButton, &temporaryHelp);
        QTRY_VERIFY(visibleHint());
        delete temporaryButton;
        QVERIFY(!visibleHint());
    }

    void noticeCarouselRotatesAndPauses()
    {
        QWidget host;
        auto *layout = new QVBoxLayout(&host);
        auto *focusTarget = new QLineEdit(&host);
        auto *carousel = new NoticeCarousel(&host);
        layout->addWidget(focusTarget);
        layout->addWidget(carousel);
        host.resize(340, 100);
        carousel->setTips({ QStringLiteral("第二条提示"), QStringLiteral("第三条提示") });
        carousel->setMessage(QStringLiteral("第一条提示"));
        carousel->setRotationInterval(70);
        host.show();
        host.activateWindow();
        focusTarget->setFocus();
        QCoreApplication::processEvents();
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(carousel, &leave);
        QTRY_VERIFY(carousel->currentPage() != 0);

        QEnterEvent enter(QPointF(5, 5), QPointF(5, 5), carousel->mapToGlobal(QPoint(5, 5)));
        QApplication::sendEvent(carousel, &enter);
        const auto page = carousel->currentPage();
        const auto tooltip = carousel->toolTip();
        QTest::qWait(320);
        QCOMPARE(carousel->currentPage(), page);
        QCOMPARE(carousel->toolTip(), tooltip);
        carousel->setMessage(QStringLiteral("设置完成"), QStringLiteral("success"));
        QCOMPARE(carousel->currentPage(), 0);
        QCOMPARE(carousel->text(), QStringLiteral("设置完成"));
        QTest::qWait(100);
        QCOMPARE(carousel->currentPage(), 0);
        QApplication::sendEvent(carousel, &leave);
        QTRY_VERIFY(carousel->currentPage() != 0);

        host.hide();
        const int hiddenPage = carousel->currentPage();
        QVERIFY(!carousel->findChild<QTimer *>(QStringLiteral("noticeRotation"))->isActive());
        QTest::qWait(180);
        QCOMPARE(carousel->currentPage(), hiddenPage);
    }

    void noticeCarouselPrioritiesAndPagination()
    {
        NoticeCarousel carousel;
        carousel.setFocusPolicy(Qt::NoFocus);
        carousel.resize(220, 26);
        carousel.setTips({ QStringLiteral("普通操作提示"), QStringLiteral("快捷键说明") });
        carousel.show();
        carousel.setMessage(QStringLiteral("正在发送文本"), QStringLiteral("busy"));
        QCOMPARE(carousel.pageCount(), 1);
        carousel.next();
        QCOMPARE(carousel.text(), QStringLiteral("正在发送文本"));
        QVERIFY(!carousel.findChild<QTimer *>(QStringLiteral("noticeRotation"))->isActive());

        const QString fullError = QStringLiteral(
            "设备连接已经中断，请检查模块供电、串口选择和数据线连接，确认设备就绪以后重新打开串口再发送播报文本。");
        QSignalSpy tones(&carousel, &NoticeCarousel::toneChanged);
        carousel.setMessage(fullError, QStringLiteral("error"));
        QVERIFY(carousel.pageCount() > 1);
        const int narrowCount = carousel.pageCount();
        QString reconstructed;
        for (int i = 0; i < narrowCount; ++i) {
            reconstructed += carousel.text();
            QCOMPARE(carousel.toolTip(), fullError);
            QCOMPARE(carousel.accessibleDescription(), fullError);
            carousel.next();
        }
        QCOMPARE(reconstructed, fullError);
        QCOMPARE(carousel.currentPage(), 0);
        QCOMPARE(tones.count(), 1); // Critical pages never switch to unrelated tips.
        QCOMPARE(tones.first().first().toString(), QStringLiteral("error"));
        carousel.resize(360, 26);
        QVERIFY(carousel.pageCount() < narrowCount);
        QCOMPARE(carousel.toolTip(), fullError);

        const QString english
            = QStringLiteral("Please check the USB cable and reopen the serial port before sending another message.");
        carousel.resize(220, 26);
        carousel.setMessage(english, QStringLiteral("error"));
        QString reconstructedEnglish;
        for (int i = 0; i < carousel.pageCount(); ++i) {
            reconstructedEnglish += carousel.text();
            carousel.next();
        }
        QCOMPARE(reconstructedEnglish, english);

        const QString multiline = QStringLiteral("串口断开\n请检查连接");
        carousel.resize(carousel.fontMetrics().horizontalAdvance(multiline) + 1, 26);
        carousel.setMessage(multiline, QStringLiteral("error"));
        for (int i = 0; i < carousel.pageCount(); ++i) {
            const int available = carousel.contentsRect().width() - (carousel.pageCount() > 1 ? 34 : 0);
            QVERIFY(carousel.fontMetrics().horizontalAdvance(carousel.text()) <= available);
            QCOMPARE(carousel.toolTip(), multiline);
            carousel.next();
        }
        carousel.resize(360, 26);

        carousel.setMessage(QStringLiteral("操作已完成"), QStringLiteral("success"));
        QCOMPARE(carousel.currentPage(), 0);
        QCOMPARE(carousel.text(), QStringLiteral("操作已完成"));
        QCOMPARE(carousel.pageCount(), 3);
        QTest::mouseClick(&carousel, Qt::LeftButton);
        QCOMPARE(carousel.currentPage(), 1);
        QTest::keyClick(&carousel, Qt::Key_Left);
        QCOMPARE(carousel.currentPage(), 0);
    }

    void noticeCarouselIntegration()
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

        carousel->setMessage(
            QStringLiteral("串口连接已中断，请检查模块供电、USB 数据线和所选端口，重新连接设备后再试。"),
            QStringLiteral("error"));
        QVERIFY(carousel->pageCount() > 1);
        QVERIFY(capture(window, "14-carousel-long-error"));
        QCOMPARE(credit->geometry(), creditGeometry);
        QCOMPARE(window.size(), QSize(500, 250));
    }
};

QTEST_MAIN(TtsTests)
#include "tst_tts.moc"
