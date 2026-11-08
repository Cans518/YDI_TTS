#pragma once

#include "core/ttsprotocol.h"
#include <QSerialPort>
#include <QTimer>
#include <cstring>

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
