#include "app/serialsender.h"
#include "core/ttsprotocol.h"
#include "ui/animatedbutton.h"
#include "ui/levelsliderwidget.h"
#include "ui/parameterpopup.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QLineEdit>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QSignalBlocker>
#include <QTimer>
#include <algorithm>

namespace {
SerialTransaction::Step statusQueryStep()
{
    return { QByteArray::fromHex("FD000121"), QStringLiteral("检查模块响应") };
}

}

SerialSender::SerialSender(QWidget *parent)
    : SerialSender(new QSerialPort, parent)
{
}

SerialSender::SerialSender(QSerialPort *port, QWidget *parent)
    : QWidget(parent)
    , serialPort(port)
{
    Q_ASSERT(serialPort); // Ownership is transferred by the injecting constructor.
    serialPort->setParent(this);
    transaction = new SerialTransaction(serialPort, this);
    setObjectName(QStringLiteral("workspace"));
    setWindowTitle(QStringLiteral("长风 TTS · 模块调试工具"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.ico")));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    setAttribute(Qt::WA_TranslucentBackground);
    background.load(QStringLiteral(":/images/background.png"));
    setFixedSize(500, 250);
    buildUi();

    connect(transaction, &SerialTransaction::progress, this, [this](const QString &description, int step, int total) {
        showFeedback(QStringLiteral("%1…  %2 / %3").arg(description).arg(step).arg(total), QStringLiteral("busy"));
        updateControls();
    });
    connect(transaction, &SerialTransaction::finished, this, &SerialSender::finishOperation);
    connect(serialPort, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (!connected || disconnecting || error == QSerialPort::NoError)
            return;
        if (error == QSerialPort::ResourceError || error == QSerialPort::DeviceNotFoundError
            || error == QSerialPort::ReadError || error == QSerialPort::WriteError) {
            const QString reason = serialPort->errorString();
            disconnectPort();
            showFeedback(QStringLiteral("设备连接中断：%1。请检查后重新连接。").arg(reason), QStringLiteral("error"));
        }
    });

    serialMonitorTimer = new QTimer(this);
    serialMonitorTimer->setObjectName(QStringLiteral("portMonitor"));
    serialMonitorTimer->setInterval(1500);
    connect(serialMonitorTimer, &QTimer::timeout, this, &SerialSender::updatePorts);
    serialMonitorTimer->start();
    updatePorts();
    updateTextState();
    showFeedback(QStringLiteral("打开串口即可播报 · 回车发送"));
}

void SerialSender::finishOperation(bool success, const QString &message)
{
    if (disconnecting)
        return;
    // Commit submitted values only after every command has been acknowledged.
    if (success) {
        if (currentOperation == Operation::Volume) {
            volumeSliderWidget->setAppliedLevel(submittedLevel);
            currentConfiguration.volume = TtsProtocol::VolumeValues[size_t(submittedLevel)];
        }
        if (currentOperation == Operation::Speed) {
            speedSliderWidget->setAppliedLevel(submittedLevel);
            currentConfiguration.speed = TtsProtocol::SpeedValues[size_t(submittedLevel)];
        }
        if (currentOperation == Operation::Sound) {
            currentConfiguration.voiceId = submittedConfiguration.voiceId;
            currentConfiguration.pitch = submittedConfiguration.pitch;
        }
        if (currentOperation == Operation::ReadConfiguration) {
            const auto configuration = TtsProtocol::parseConfiguration(transaction->response());
            if (!configuration) {
                disconnectPort();
                showFeedback(QStringLiteral("读取配置失败：返回数据不完整。"), QStringLiteral("error"));
                return;
            }
            currentConfiguration = *configuration;
            volumeSliderWidget->setReadbackLevel(
                TtsProtocol::nearestLevel(configuration->volume, TtsProtocol::VolumeValues), configuration->volume);
            speedSliderWidget->setReadbackLevel(
                TtsProtocol::nearestLevel(configuration->speed, TtsProtocol::SpeedValues), configuration->speed);
            successMessage = QStringLiteral("已读取：音量 %1 · 语速 %2 · 音调 %3 · %4")
                                 .arg(configuration->volume)
                                 .arg(configuration->speed)
                                 .arg(configuration->pitch)
                                 .arg(TtsProtocol::voiceIdLabel(configuration->voiceId));
        }
    } else {
        // An untagged late ACK must never be reused by a subsequent operation.
        disconnectPort();
    }
    currentOperation = Operation::None;
    if (success && !validText && !textInput->text().trimmed().isEmpty()) {
        updateTextState();
    } else {
        updateControls();
        showFeedback(success ? successMessage : message, success ? QStringLiteral("success") : QStringLiteral("error"));
    }
}

SerialSender::~SerialSender()
{
    disconnectPort();
}

void SerialSender::closeEvent(QCloseEvent *event)
{
    disconnectPort();
    event->accept();
}

void SerialSender::updatePorts()
{
    auto ports = QSerialPortInfo::availablePorts();
    std::sort(ports.begin(), ports.end(), [](const auto &a, const auto &b) { return a.portName() < b.portName(); });
    QStringList signature;
    QStringList names;
    for (const auto &port : ports) {
        names.append(port.portName());
        signature.append(port.portName() + QChar('\t') + port.description());
    }
    if (connected && !names.contains(serialPort->portName())) {
        disconnectPort();
        showFeedback(QStringLiteral("设备已拔出，请重新连接。"), QStringLiteral("error"));
    }
    if (signature == currentPortSignature && portCombo->count() > 0)
        return;
    currentPortSignature = signature;
    const QString selected = connected ? serialPort->portName() : preferredPort;
    const QSignalBlocker blocker(portCombo);
    portCombo->clear();
    for (const auto &port : ports) {
        const QString text = port.description().isEmpty()
            ? port.portName()
            : QStringLiteral("%1 · %2").arg(port.portName(), port.description());
        portCombo->addItem(text, port.portName());
    }
    if (portCombo->count() == 0)
        portCombo->addItem(QStringLiteral("未发现串口设备"), QString());
    const int selectedIndex = portCombo->findData(selected);
    if (selectedIndex >= 0)
        portCombo->setCurrentIndex(selectedIndex);
    portCombo->setToolTip(portCombo->currentText());
    updateControls();
}

void SerialSender::toggleSerialPort()
{
    if (connected) {
        disconnectPort();
        showFeedback(QStringLiteral("连接已断开，可重新选择串口设备。"));
        return;
    }
    const QString name = portCombo->currentData().toString();
    if (name.isEmpty()) {
        showFeedback(QStringLiteral("未发现串口设备，请检查 USB 连接与驱动。"), QStringLiteral("error"));
        return;
    }
    serialPort->setPortName(name);
    serialPort->setBaudRate(QSerialPort::Baud115200);
    serialPort->setDataBits(QSerialPort::Data8);
    serialPort->setParity(QSerialPort::NoParity);
    serialPort->setStopBits(QSerialPort::OneStop);
    serialPort->setFlowControl(QSerialPort::NoFlowControl);
    if (!serialPort->open(QIODevice::ReadWrite)) {
        showFeedback(QStringLiteral("无法打开 %1：%2").arg(name, serialPort->errorString()), QStringLiteral("error"));
        return;
    }
    connected = true;
    preferredPort = name;
    updateControls();
    showFeedback(QStringLiteral("已连接 %1。可发送播报，或先测试连接。").arg(name), QStringLiteral("success"));
}

void SerialSender::disconnectPort()
{
    soundPopup->hide();
    // cancel() emits finished synchronously; suppress its normal error path while closing.
    disconnecting = true;
    connected = false;
    transaction->cancel();
    if (serialPort->isOpen())
        serialPort->close();
    currentOperation = Operation::None;
    volumeSliderWidget->setAppliedLevel(-1);
    speedSliderWidget->setAppliedLevel(-1);
    disconnecting = false;
    updateControls();
}

QVector<SerialTransaction::Step> SerialSender::initializationSteps() const
{
    return { { TtsProtocol::textFrame(TtsProtocol::pitchValueCommand(currentConfiguration.pitch)),
                 QStringLiteral("设置音调") },
        { TtsProtocol::textFrame(TtsProtocol::voiceIdCommand(currentConfiguration.voiceId)),
            QStringLiteral("设置音色") } };
}

void SerialSender::beginOperation(
    Operation operation, const QVector<SerialTransaction::Step> &steps, const QString &success, int timeoutMs)
{
    if (transaction->isBusy())
        return;
    if (!connected || !serialPort->isOpen()) {
        showFeedback(QStringLiteral("请先连接串口设备。"), QStringLiteral("error"));
        return;
    }
    currentOperation = operation;
    successMessage = success;
    if (!transaction->start(steps, timeoutMs)) {
        currentOperation = Operation::None;
        showFeedback(QStringLiteral("无法启动操作，请重新连接设备。"), QStringLiteral("error"));
    }
    updateControls();
}

void SerialSender::sendData()
{
    if (transaction->isBusy())
        return;
    const auto encoded = TtsProtocol::encodeText(textInput->text());
    if (!encoded.valid()) {
        showFeedback(encoded.error, QStringLiteral("error"));
        notice->setProperty("validation", true);
        textInput->setFocus();
        return;
    }
    auto steps = initializationSteps();
    steps.append({ TtsProtocol::textFrame(encoded.bytes), QStringLiteral("发送播报文本") });
    beginOperation(Operation::Speech, steps, QStringLiteral("模块已接收播报文本。"));
}

void SerialSender::testConnection()
{
    auto steps = initializationSteps();
    steps.prepend(statusQueryStep());
    beginOperation(Operation::Test, steps, QStringLiteral("连接测试通过，模块已就绪。"));
}

void SerialSender::applyVolumeLevel()
{
    if (transaction->isBusy())
        return;
    submittedLevel = volumeSliderWidget->currentLevel();
    auto steps = initializationSteps();
    steps.prepend(statusQueryStep());
    steps.append({ TtsProtocol::textFrame(TtsProtocol::volumeCommand(submittedLevel)), QStringLiteral("应用音量") });
    beginOperation(Operation::Volume, steps, QStringLiteral("音量已设为第 %1 档。").arg(submittedLevel + 1));
}

void SerialSender::applySpeedLevel()
{
    if (transaction->isBusy())
        return;
    submittedLevel = speedSliderWidget->currentLevel();
    auto steps = initializationSteps();
    steps.prepend(statusQueryStep());
    steps.append({ TtsProtocol::textFrame(TtsProtocol::speedCommand(submittedLevel)), QStringLiteral("应用语速") });
    beginOperation(Operation::Speed, steps, QStringLiteral("语速已设为第 %1 档。").arg(submittedLevel + 1));
}

void SerialSender::applySound(int voiceId, int pitchLevel)
{
    if (transaction->isBusy() || TtsProtocol::voiceIdCommand(voiceId).isEmpty()
        || TtsProtocol::pitchCommand(pitchLevel).isEmpty())
        return;
    submittedConfiguration = currentConfiguration;
    submittedConfiguration.voiceId = voiceId;
    submittedConfiguration.pitch = TtsProtocol::PitchValues[size_t(pitchLevel)];
    const QByteArray parameters = TtsProtocol::pitchCommand(pitchLevel) + TtsProtocol::voiceIdCommand(voiceId);
    beginOperation(Operation::Sound,
        { statusQueryStep(), { TtsProtocol::textFrame(parameters), QStringLiteral("设置音色和音调") } },
        QStringLiteral("已设置%1，音调第 %2 档。").arg(TtsProtocol::voiceIdLabel(voiceId)).arg(pitchLevel + 1));
}

void SerialSender::readConfiguration()
{
    if (transaction->isBusy())
        return;
    soundPopup->hide();
    beginOperation(Operation::ReadConfiguration,
        { { TtsProtocol::readConfigurationFrame(), QStringLiteral("读取配置"),
            SerialTransaction::Reply::Configuration } },
        QStringLiteral("配置已读取。"), 2000);
}
