#include "core/serialtransaction.h"
#include "core/ttsprotocol.h"
#include <algorithm>

namespace {
constexpr int FrameGapMs = 35;
}

SerialTransaction::SerialTransaction(QIODevice *device, QObject *parent)
    : QObject(parent)
    , m_device(device)
{
    m_deadline.setSingleShot(true);
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        if (!m_busy)
            return;
        finish(false, QStringLiteral("%1超时，请检查模块供电与接线后重新连接。").arg(m_steps.at(m_index).description));
    });
    if (!m_device)
        return;
    connect(m_device, &QObject::destroyed, this, [this] {
        m_device.clear();
        cancel(QStringLiteral("连接设备已释放，操作已取消。"));
    });
    connect(m_device, &QIODevice::readyRead, this, &SerialTransaction::receive);
    connect(m_device, &QIODevice::aboutToClose, this, [this] { cancel(QStringLiteral("连接已断开，操作已取消。")); });
}

bool SerialTransaction::start(const QVector<Step> &steps, int timeoutMs)
{
    if (m_busy || steps.isEmpty() || !m_device || !m_device->isOpen() || !m_device->isReadable()
        || !m_device->isWritable()
        || std::any_of(steps.cbegin(), steps.cend(), [](const Step &step) { return step.frame.isEmpty(); }))
        return false;
    m_steps = steps;
    m_response.clear();
    m_index = 0;
    m_timeoutMs = qMax(1, timeoutMs);
    m_busy = true;
    ++m_generation;
    writeNext();
    return true;
}

void SerialTransaction::writeNext()
{
    if (!m_busy)
        return;
    if (m_index == m_steps.size()) {
        finish(true);
        return;
    }
    if (!m_device || !m_device->isOpen()) {
        finish(false, QStringLiteral("串口已断开，请重新连接。"));
        return;
    }
    // Cancellation, write errors and rapid reconnects must not bypass UART spacing.
    const qint64 elapsed = m_frameGap.isValid() ? m_frameGap.elapsed() : FrameGapMs;
    if (elapsed < FrameGapMs) {
        const auto generation = m_generation;
        QTimer::singleShot(int(FrameGapMs - elapsed), Qt::PreciseTimer, this, [this, generation] {
            if (m_busy && m_generation == generation)
                writeNext();
        });
        return;
    }
    m_device->readAll(); // Discard residual acknowledgements before the next command.
    m_received.clear();
    const auto step = m_steps.at(m_index);
    // progress() may run arbitrary direct slots. No command has been sent yet,
    // so replies delivered by those slots must not acknowledge this step.
    m_waiting = false;
    const auto generation = m_generation;
    emit progress(step.description, m_index + 1, int(m_steps.size()));
    if (m_busy && m_generation == generation) {
        m_waiting = true;
        m_deadline.start(m_timeoutMs);
        m_frameGap.start();
        const auto written = m_device->write(step.frame);
        if (m_busy && m_generation == generation && written != step.frame.size())
            finish(false, QStringLiteral("写入失败：%1").arg(m_device->errorString()));
    }
}

void SerialTransaction::receive()
{
    if (!m_device)
        return;
    const QByteArray data = m_device->readAll();
    if (!m_busy || !m_waiting)
        return;
    m_received.append(data);
    qsizetype offset = 0;
    while (offset < m_received.size()) {
        const auto code = quint8(m_received.at(offset));
        if (code == 0x5f || code == 0x5e) {
            const qsizetype length = code == 0x5f ? 18 : 13;
            if (m_received.size() - offset < length)
                break;
            const QByteArray packet = m_received.mid(offset, length);
            offset += length;
            if (code == 0x5f && m_steps.at(m_index).reply == Reply::Configuration) {
                if (!TtsProtocol::parseConfiguration(packet)) {
                    finish(false, QStringLiteral("读取配置失败：模块返回的参数超出有效范围。"));
                    return;
                }
                m_response = packet;
                completeStep();
                return;
            }
            // Bytes inside parameter blocks must never be interpreted as ACKs.
            continue;
        }
        ++offset;
        if (code == 0x45) {
            finish(false, QStringLiteral("%1失败：模块拒绝了命令。").arg(m_steps.at(m_index).description));
            return;
        }
        if (code == 0x4f && m_steps.at(m_index).reply == Reply::Acknowledgement) {
            completeStep();
            return;
        }
    }
    m_received.remove(0, offset);
}

void SerialTransaction::completeStep()
{
    m_deadline.stop();
    m_waiting = false;
    m_received.clear();
    m_frameGap.start();
    const auto generation = m_generation;
    // Leave >30 ms between UART frames. Waiting 35 ms after
    // the ACK also protects a new operation started by the finished callback.
    // Consume this entire reply; never reuse its ACK for another command.
    QTimer::singleShot(FrameGapMs, Qt::PreciseTimer, this, [this, generation] {
        if (m_busy && m_generation == generation) {
            ++m_index;
            writeNext();
        }
    });
}

void SerialTransaction::cancel(const QString &reason)
{
    if (m_busy)
        finish(false, reason);
}

void SerialTransaction::finish(bool success, const QString &message)
{
    if (!m_busy)
        return;
    m_deadline.stop();
    m_waiting = false;
    m_busy = false;
    ++m_generation;
    m_steps.clear();
    m_received.clear();
    if (!success)
        m_response.clear();
    emit finished(success, message);
}
