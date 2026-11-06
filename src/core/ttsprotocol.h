#ifndef TTSPROTOCOL_H
#define TTSPROTOCOL_H

#include <QByteArray>
#include <QString>
#include <array>
#include <optional>

namespace TtsProtocol {
constexpr qsizetype MaxTextBytes = 500;
// Supported module voice IDs and five-level control values.
constexpr std::array<int, 8> VoiceIds { 3, 51, 52, 53, 54, 55, 56, 57 };
constexpr std::array<int, 5> VolumeValues { 2, 4, 6, 8, 10 };
constexpr std::array<int, 5> SpeedValues { 6, 12, 18, 24, 30 };
constexpr std::array<int, 5> PitchValues { 2, 4, 6, 8, 10 };
constexpr int DefaultVoiceIndex = 6; // Retain the original application's [m56].

struct Configuration
{
    int volume = 2;
    int speed = 6;
    int pitch = 9;
    int voiceId = VoiceIds[DefaultVoiceIndex];
};

struct EncodedText
{
    QByteArray bytes;
    QString error;
    bool valid() const { return error.isEmpty(); }
};

EncodedText encodeText(const QString &text);
QByteArray textFrame(const QByteArray &payload);
QByteArray volumeCommand(int level);
QByteArray speedCommand(int level);
QByteArray voiceCommand(int index);
QByteArray pitchCommand(int level);
QByteArray pitchValueCommand(int value);
QByteArray voiceIdCommand(int voiceId);
QByteArray readConfigurationFrame();
std::optional<Configuration> parseConfiguration(const QByteArray &reply);
int nearestLevel(int value, const std::array<int, 5> &values);
QString voiceLabel(int index);
QString voiceIdLabel(int voiceId);
}

#endif
