#include "core/ttsprotocol.h"

#include <QStringEncoder>
#include <QStringList>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

TtsProtocol::EncodedText TtsProtocol::encodeText(const QString &text)
{
    if (text.trimmed().isEmpty())
        return { {}, QStringLiteral("请输入要朗读的内容。") };

    QByteArray bytes;
#ifdef Q_OS_WIN
    // The module's 0x01 text encoding is GBK, independent of the Windows locale.
    BOOL substituted = FALSE;
    const auto *utf16 = reinterpret_cast<const wchar_t *>(text.utf16());
    const int length
        = WideCharToMultiByte(936, WC_NO_BEST_FIT_CHARS, utf16, int(text.size()), nullptr, 0, nullptr, &substituted);
    if (length <= 0 || substituted)
        return { {}, QStringLiteral("内容包含模块不支持的字符，请移除表情或特殊字符。") };
    bytes.resize(length);
    substituted = FALSE;
    const int written = WideCharToMultiByte(
        936, WC_NO_BEST_FIT_CHARS, utf16, int(text.size()), bytes.data(), length, nullptr, &substituted);
    if (written != length || substituted)
        return { {}, QStringLiteral("内容包含模块不支持的字符，请移除表情或特殊字符。") };
#else
    QStringEncoder encoder("GBK");
    if (!encoder.isValid())
        return { {}, QStringLiteral("当前系统缺少 GBK 编码支持。") };
    bytes = encoder(text);
    if (encoder.hasError())
        return { {}, QStringLiteral("内容包含模块不支持的字符，请移除表情或特殊字符。") };
#endif
    if (bytes.size() > MaxTextBytes)
        return { bytes, QStringLiteral("内容超过 500 字节，请缩短后发送。") };
    return { bytes, {} };
}

QByteArray TtsProtocol::textFrame(const QByteArray &payload)
{
    if (payload.isEmpty() || payload.size() > MaxTextBytes)
        return {};
    const auto length = quint16(payload.size() + 2);
    QByteArray frame;
    frame.reserve(payload.size() + 5);
    frame.append(char(0xfd));
    frame.append(char(length >> 8));
    frame.append(char(length & 0xff));
    frame.append(char(0x01));
    frame.append(char(0x01));
    frame.append(payload);
    return frame;
}

QByteArray TtsProtocol::volumeCommand(int level)
{
    return "[v" + QByteArray::number((qBound(0, level, 4) + 1) * 2) + ']';
}

QByteArray TtsProtocol::speedCommand(int level)
{
    return "[s" + QByteArray::number((qBound(0, level, 4) + 1) * 6) + ']';
}

QByteArray TtsProtocol::voiceCommand(int index)
{
    if (index < 0 || index >= int(VoiceIds.size()))
        return {};
    return "[m" + QByteArray::number(VoiceIds[size_t(index)]) + ']';
}

QByteArray TtsProtocol::pitchCommand(int level)
{
    if (level < 0 || level >= int(PitchValues.size()))
        return {};
    return "[t" + QByteArray::number(PitchValues[size_t(level)]) + ']';
}

QByteArray TtsProtocol::pitchValueCommand(int value)
{
    if (value < 1 || value > 10)
        return {};
    return "[t" + QByteArray::number(value) + ']';
}

QByteArray TtsProtocol::voiceIdCommand(int voiceId)
{
    if (voiceId < 0 || voiceId > 255)
        return {};
    return "[m" + QByteArray::number(voiceId) + ']';
}

QByteArray TtsProtocol::readConfigurationFrame()
{
    return QByteArray::fromHex("fd000105");
}

std::optional<TtsProtocol::Configuration> TtsProtocol::parseConfiguration(const QByteArray &reply)
{
    // Configuration reply: 5F + 17 payload bytes. The first two
    // payload bytes are the encoding; s/v/t/m follow as individual bytes.
    if (reply.size() != 18 || quint8(reply.at(0)) != 0x5f)
        return std::nullopt;
    Configuration result;
    result.speed = quint8(reply.at(3));
    result.volume = quint8(reply.at(4));
    result.pitch = quint8(reply.at(5));
    result.voiceId = quint8(reply.at(6));
    if (result.speed < 1 || result.speed > 30 || result.volume > 10 || result.pitch < 1 || result.pitch > 10)
        return std::nullopt;
    return result;
}

int TtsProtocol::nearestLevel(int value, const std::array<int, 5> &values)
{
    int closest = 0;
    for (int i = 1; i < int(values.size()); ++i)
        if (qAbs(value - values[size_t(i)]) < qAbs(value - values[size_t(closest)]))
            closest = i;
    return closest; // Equal distances select the lower level.
}

QString TtsProtocol::voiceLabel(int index)
{
    static const QStringList categories { QStringLiteral("女声"), QStringLiteral("男声"), QStringLiteral("男声"),
        QStringLiteral("女声"), QStringLiteral("特殊音色"), QStringLiteral("女童声"), QStringLiteral("男童声"),
        QStringLiteral("男童声") };
    if (index < 0 || index >= int(VoiceIds.size()))
        return {};
    return QStringLiteral("音色 %1 · %2").arg(index + 1, 2, 10, QLatin1Char('0')).arg(categories.at(index));
}

QString TtsProtocol::voiceIdLabel(int voiceId)
{
    for (int i = 0; i < int(VoiceIds.size()); ++i)
        if (VoiceIds[size_t(i)] == voiceId)
            return voiceLabel(i);
    return QStringLiteral("模块音色 · ID %1").arg(voiceId);
}
