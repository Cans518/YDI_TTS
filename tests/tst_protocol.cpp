#include "support/memoryport.h"
#include "ttstests.h"
#include <QBuffer>
#include <QElapsedTimer>
#include <limits>

void TtsTests::protocolLength_data()
{
    QTest::addColumn<int>("size");
    QTest::newRow("small") << 1;
    QTest::newRow("last-low-byte") << 253;
    QTest::newRow("first-high-byte") << 254;
    QTest::newRow("maximum") << 500;
}

void TtsTests::protocolLength()
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

void TtsTests::encodingAndLimits()
{
    const auto chinese = TtsProtocol::encodeText(QStringLiteral("中文ABC"));
    QVERIFY2(chinese.valid(), qPrintable(chinese.error));
    QCOMPARE(chinese.bytes, QByteArray::fromHex("d6d0cec4414243"));
    QVERIFY(!TtsProtocol::encodeText(QString::fromUtf8("🙂")).valid());
    QVERIFY(!TtsProtocol::encodeText(QString(QChar(0xd800))).valid());
    QVERIFY(!TtsProtocol::encodeText(QString(QChar(0xdc00))).valid());
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

void TtsTests::voiceAndPitchProtocol()
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

void TtsTests::configurationProtocolAndMapping()
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

void TtsTests::configurationRepliesAreFramed()
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

void TtsTests::asynchronousSequence()
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

void TtsTests::timeoutAndWriteFailure()
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

void TtsTests::cancelAndRestart()
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

void TtsTests::reentrantProgressCancellation()
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

void TtsTests::transactionRejectsInvalidSteps()
{
    MemoryPort port;
    QVERIFY(port.open(QIODevice::ReadWrite));
    SerialTransaction transaction(&port);
    QSignalSpy finished(&transaction, &SerialTransaction::finished);
    QVERIFY(!transaction.start({}));
    QVERIFY(!transaction.start({ { "first", "first" }, { {}, "empty" } }));
    QVERIFY(port.writes.isEmpty()); // Validate the entire batch before sending anything.
    QVERIFY(!transaction.isBusy());
    QCOMPARE(finished.count(), 0);
    QVERIFY(transaction.start({ { "valid", "valid" } }));
    port.feed("\x4f");
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(finished.first()[0].toBool());
}

void TtsTests::transactionDeviceLifetime()
{
    MemoryPort writeOnly;
    QVERIFY(writeOnly.open(QIODevice::WriteOnly));
    SerialTransaction unreadable(&writeOnly);
    QVERIFY(!unreadable.start({ { "command", "command" } }));
    SerialTransaction missing(nullptr);
    QVERIFY(!missing.start({ { "command", "command" } }));
    // QBuffer destruction does not rely on the mock serial port's close override.
    auto *device = new QBuffer;
    QVERIFY(device->open(QIODevice::ReadWrite));
    SerialTransaction transaction(device);
    QSignalSpy finished(&transaction, &SerialTransaction::finished);
    QVERIFY(transaction.start({ { "command", "command" } }));
    delete device;
    QCOMPARE(finished.count(), 1);
    QVERIFY(!finished.first()[0].toBool());
    QVERIFY(!transaction.isBusy());
    QVERIFY(!transaction.start({ { "retry", "retry" } }));
    QTest::qWait(50);
    QCOMPARE(finished.count(), 1);
}

void TtsTests::progressCannotAcknowledgeUnsentFrame()
{
    MemoryPort port;
    QVERIFY(port.open(QIODevice::ReadWrite));
    SerialTransaction transaction(&port);
    QSignalSpy finished(&transaction, &SerialTransaction::finished);
    connect(&transaction, &SerialTransaction::progress, &port, [&] { port.feed("\x4f"); });
    QVERIFY(transaction.start({ { "command", "command" } }));
    QCOMPARE(port.writes, QList<QByteArray> { "command" });
    QTest::qWait(60); // Longer than the inter-command gap: a stale ACK would finish now.
    QCOMPARE(finished.count(), 0);
    QVERIFY(transaction.isBusy());
    port.feed("\x4f");
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(finished.first()[0].toBool());
}

void TtsTests::extremeParameterMapping()
{
    QCOMPARE(TtsProtocol::nearestLevel(std::numeric_limits<int>::min(), TtsProtocol::PitchValues), 0);
    QCOMPARE(TtsProtocol::nearestLevel(std::numeric_limits<int>::max(), TtsProtocol::PitchValues), 4);
    QCOMPARE(TtsProtocol::nearestLevel(3, TtsProtocol::PitchValues), 0);
    QCOMPARE(TtsProtocol::volumeCommand(-1), QByteArray("[v2]"));
    QCOMPARE(TtsProtocol::volumeCommand(99), QByteArray("[v10]"));
    QCOMPARE(TtsProtocol::speedCommand(-1), QByteArray("[s6]"));
    QCOMPARE(TtsProtocol::speedCommand(99), QByteArray("[s30]"));
}
