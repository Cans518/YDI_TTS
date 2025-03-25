#include "SerialSender.h"

SerialSender::SerialSender(QWidget *parent)
    : QWidget(parent), serialPort(new QSerialPort(this)), isSerialOpen(false) {
    // 设置窗口属性
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    setAttribute(Qt::WA_TranslucentBackground); // 设置窗口背景透明
    setWindowTitle("串口发送工具");
    setFixedSize(300, 200); // 调整窗口大小
    setWindowIcon(QIcon(":/1.ico"));

    // 设置主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QHBoxLayout *titleLayout = new QHBoxLayout();
    QLabel *titleLabel = new QLabel("TTS模块调试工具");
    titleLabel->setFont(QFont(":/font/LXGWWenKaiMonoGBScreen.ttf", 12, QFont::Bold));
    // 居中
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLayout->addWidget(titleLabel);
    mainLayout->addLayout(titleLayout);

    // 串口选择部分
    QHBoxLayout *portLayout = new QHBoxLayout();
    portLayout->setContentsMargins(0, 0, 0, 0); // 去除布局边距

    portLabel = new QLabel("选择串口:");
    portLabel->setFont(QFont(":/font/LXGWWenKaiMonoGBScreen.ttf", 10,QFont::Bold));
    // 增加半透明底色
    portLabel->setStyleSheet("background-color: rgba(255, 255, 255, 100);border-radius: 5px;");
    portLayout->addWidget(portLabel);

    portCombo = new QComboBox();
    portCombo->setFont(QFont(":/font/LXGWWenKaiMonoGBScreen.ttf", 10));
    portCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    updatePorts();
    portLayout->addWidget(portCombo);
    portLayout->addSpacing(12);

    // 红点状态
    QWidget *statusWidget = new QWidget(this);
    statusWidget->setFixedSize(10, 10);
    portLayout->addWidget(statusWidget);

    // 打开/关闭串口按钮
    toggleButton = new QPushButton("打开串口");
    toggleButton->setFont(QFont(":/font/LXGWWenKaiMonoGBScreen.ttf", 10, QFont::Bold));
    toggleButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #4CAF50;"
        "   color: white;"
        "   padding: 6px;"
        "   border-radius: 5px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #45A049;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #3E8E41;"
        "}"
        );
    connect(toggleButton, &QPushButton::clicked, this, &SerialSender::toggleSerialPort);
    portLayout->addWidget(toggleButton);

    mainLayout->addLayout(portLayout);

    // 文本输入部分
    QHBoxLayout *inputLayout = new QHBoxLayout();
    textInput = new QLineEdit();
    textInput->setFont(QFont(":/font/LXGWWenKaiMonoGBScreen.ttf", 10));
    textInput->setPlaceholderText("请输入要发送的内容...");
    textInput->setStyleSheet("padding: 5px; border: 1px solid #CCCCCC; border-radius: 4px;");
    connect(textInput, &QLineEdit::returnPressed, this, &SerialSender::sendData);
    inputLayout->addWidget(textInput);

    sendButton = new QPushButton("发送");
    sendButton->setFont(QFont(":/font/LXGWWenKaiMonoGBScreen.ttf", 10, QFont::Bold));
    sendButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #008CBA;"
        "   color: white;"
        "   padding: 6px;"
        "   border-radius: 5px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #007BB5;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #005F8A;"
        "}"
        );
    connect(sendButton, &QPushButton::clicked, this, &SerialSender::sendData);
    inputLayout->addWidget(sendButton);

    mainLayout->addLayout(inputLayout);

    // 操作按钮部分
    QHBoxLayout *buttonLayout = new QHBoxLayout();

    testButton = new QPushButton("测试连接");
    testButton->setFont(QFont(":/font/LXGWWenKaiMonoGBScreen.ttf", 10, QFont::Bold));
    testButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #FF9800;"
        "   color: white;"
        "   padding: 6px;"
        "   border-radius: 5px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #FB8C00;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #EF6C00;"
        "}"
        );
    connect(testButton, &QPushButton::clicked, this, &SerialSender::testConnection);
    buttonLayout->addWidget(testButton);

    defaultSpeedButton = new QPushButton("默认语速");
    defaultSpeedButton->setFont(QFont(":/font/LXGWWenKaiMonoGBScreen.ttf", 10, QFont::Bold));
    defaultSpeedButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #4CAF50;"
        "   color: white;"
        "   padding: 6px;"
        "   border-radius: 5px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #45A049;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #3E8E41;"
        "}"
        );
    connect(defaultSpeedButton, &QPushButton::clicked, this, &SerialSender::setDefaultSpeed);
    buttonLayout->addWidget(defaultSpeedButton);

    fastSpeedButton = new QPushButton("快速语速");
    fastSpeedButton->setFont(QFont(":/font/LXGWWenKaiMonoGBScreen.ttf", 10, QFont::Bold));
    fastSpeedButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #F44336;"
        "   color: white;"
        "   padding: 6px;"
        "   border-radius: 5px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #E53935;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #D32F2F;"
        "}"
        );
    connect(fastSpeedButton, &QPushButton::clicked, this, &SerialSender::setFastSpeed);
    buttonLayout->addWidget(fastSpeedButton);

    mainLayout->addLayout(buttonLayout);

    QHBoxLayout *newButtonLayout = new QHBoxLayout();

    closeButton = new QPushButton("关闭程序");
    closeButton->setFont(QFont(":/font/LXGWWenKaiMonoGBScreen.ttf", 10, QFont::Bold));
    closeButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #FF5722;"
        "   color: white;"
        "   padding: 6px;"
        "   border-radius: 5px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #F4511E;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #E64A19;"
        "}"
        );
    connect(closeButton, &QPushButton::clicked, this, &SerialSender::closeApplication);
    newButtonLayout->addWidget(closeButton);

    standardVolumeButton = new QPushButton("标准音量");
    standardVolumeButton->setFont(QFont(":/font/LXGWWenKaiMonoGBScreen.ttf", 10, QFont::Bold));
    standardVolumeButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #4CAF50;"
        "   color: white;"
        "   padding: 6px;"
        "   border-radius: 5px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #45A049;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #3E8E41;"
        "}"
        );
    connect(standardVolumeButton, &QPushButton::clicked, this, &SerialSender::setStandardVolume);
    newButtonLayout->addWidget(standardVolumeButton);

    maxVolumeButton = new QPushButton("最大音量");
    maxVolumeButton->setFont(QFont(":/font/LXGWWenKaiMonoGBScreen.ttf", 10, QFont::Bold));
    maxVolumeButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #F44336;"
        "   color: white;"
        "   padding: 6px;"
        "   border-radius: 5px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #E53935;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #D32F2F;"
        "}"
        );
    connect(maxVolumeButton, &QPushButton::clicked, this, &SerialSender::setMaxVolume);
    newButtonLayout->addWidget(maxVolumeButton);

    mainLayout->addLayout(newButtonLayout);

    // 添加作者信息右对齐
    QLabel *mail_Label = new QLabel("Email:xiemaths@outlook.com");
    // 字体倾斜
    mail_Label->setFont(QFont(":/font/LXGWWenKaiMonoGBScreen.ttf", 8));
    mail_Label->setAlignment(Qt::AlignRight);
    mainLayout->addWidget(mail_Label);


    setLayout(mainLayout);
}

void SerialSender::paintEvent(QPaintEvent *event) {
    QPainter painter(this);

    // 绘制背景图片
    QPixmap background(":/background/1.png");
    painter.drawPixmap(rect(), background);
    // 给图片倒圆角
    QBitmap mask(this->size());
    mask.fill();
    QPainter maskPainter(&mask);
    maskPainter.setPen(Qt::NoPen);
    maskPainter.setBrush(Qt::white);
    maskPainter.drawRoundedRect(rect(), 15, 15);
    setMask(mask);
    // 图片背景透明
    setAttribute(Qt::WA_TranslucentBackground, true);

    // 绘制圆角背景
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(245, 245, 245,50)); // 半透明背景
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), 15, 15); // 圆角半径为15

    // 绘制红点状态
    int x = portCombo->geometry().right() + 10;
    int y = portCombo->geometry().top() + (portCombo->geometry().height() - 10) / 2;

    painter.setBrush(isSerialOpen ? Qt::red : Qt::gray);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(x, y, 10, 10);
}

void SerialSender::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void SerialSender::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - dragPosition);
        event->accept();
    }
}



SerialSender::~SerialSender() {
    if (serialPort->isOpen()) {
        serialPort->close();
    }
    delete serialPort;
}

void SerialSender::updatePorts() {
    portCombo->clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const auto &port : ports) {
        portCombo->addItem(port.portName());
    }
}

void SerialSender::toggleSerialPort() {
    if (isSerialOpen) {
        serialPort->close();
        isSerialOpen = false;
        toggleButton->setText("打开串口");
        toggleButton->setStyleSheet("background-color: #4CAF50; color: white; padding: 6px;");
        update();
    } else {
        QString selectedPort = portCombo->currentText();

        if (selectedPort.isEmpty()) {
            QMessageBox::warning(this, "警告", "未选择串口！");
            return;
        }

        serialPort->setPortName(selectedPort);
        serialPort->setBaudRate(QSerialPort::Baud115200);

        if (serialPort->open(QIODevice::ReadWrite)) {
            isSerialOpen = true;
            toggleButton->setText("关闭串口");
            toggleButton->setStyleSheet("background-color: #F44336; color: white; padding: 6px;");
            update();
        } else {
            QMessageBox::critical(this, "错误", "打开串口失败！");
        }
    }
}

void SerialSender::sendData() {
    if (!isSerialOpen) {
        QMessageBox::warning(this, "警告", "串口未打开！");
        return;
    }

    QString text = textInput->text();

    if (text.isEmpty()) {
        QMessageBox::warning(this, "警告", "未输入内容！");
        return;
    }

    QByteArray gbkEncoded = text.toLocal8Bit();

    if (gbkEncoded.size() > 500) {
        QMessageBox::warning(this, "警告", "编码后的内容超过500字节，无法发送！");
        return;
    }

    QByteArray frame;
    frame.append(static_cast<char>(0xFD));
    frame.append(static_cast<char>(0x00));
    frame.append(static_cast<char>(gbkEncoded.size() + 2));
    frame.append(static_cast<char>(0x01));
    frame.append(static_cast<char>(0x01));
    frame.append(gbkEncoded);

    serialPort->write(frame);
}

bool SerialSender::waitForResponse(const QByteArray &expectedData, int timeoutMs) {
    QByteArray receivedData;
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs) {
        if (serialPort->waitForReadyRead(10)) {
            receivedData.append(serialPort->readAll());
            if (receivedData.contains(expectedData)) {
                return true;
            }
        }
    }
    return false;
}

void SerialSender::testConnection() {
    if (!isSerialOpen) {
        QMessageBox::warning(this, "警告", "串口未打开！");
        return;
    }

    QByteArray testFrame = QByteArray::fromHex("FD000121");
    serialPort->write(testFrame);

    if (waitForResponse(QByteArray::fromHex("4F"), 1000)) {
        QMessageBox::information(this, "测试连接", "连接测试成功！");
    } else {
        QMessageBox::critical(this, "测试连接", "连接测试失败！");
    }
}

void SerialSender::setDefaultSpeed() {
    if (!isSerialOpen) {
        QMessageBox::warning(this, "警告", "串口未打开！");
        return;
    }

    QByteArray prepareFrame = QByteArray::fromHex("FD000121");
    serialPort->write(prepareFrame);

    if (!waitForResponse(QByteArray::fromHex("4F"), 1000)) {
        QMessageBox::critical(this, "默认语速", "准备阶段失败！");
        return;
    }

    QByteArray defaultSpeedFrame = QByteArray::fromHex("FD000601015B73395D");
    serialPort->write(defaultSpeedFrame);

    if (!waitForResponse(QByteArray::fromHex("4F"), 1000)) {
        QMessageBox::critical(this, "默认语速", "设置默认语速失败！");
    } else {
        QMessageBox::information(this, "默认语速", "设置默认语速成功！");
    }
}

void SerialSender::setFastSpeed() {
    if (!isSerialOpen) {
        QMessageBox::warning(this, "警告", "串口未打开！");
        return;
    }

    QByteArray prepareFrame = QByteArray::fromHex("FD000121");
    serialPort->write(prepareFrame);

    if (!waitForResponse(QByteArray::fromHex("4F"), 1000)) {
        QMessageBox::critical(this, "快速语速", "准备阶段失败！");
        return;
    }

    QByteArray fastSpeedFrame = QByteArray::fromHex("FD000706015B7333305D");
    serialPort->write(fastSpeedFrame);

    if (!waitForResponse(QByteArray::fromHex("4F"), 1000)) {
        QMessageBox::critical(this, "快速语速", "设置快速语速失败！");
    } else {
        QMessageBox::information(this, "快速语速", "设置快速语速成功！");
    }
}

void SerialSender::setStandardVolume() {
    if (!isSerialOpen) {
        QMessageBox::warning(this, "警告", "串口未打开！");
        return;
    }

    QByteArray prepareFrame = QByteArray::fromHex("FD000121");
    serialPort->write(prepareFrame);

    if (!waitForResponse(QByteArray::fromHex("4F"), 1000)) {
        QMessageBox::critical(this, "标准音量", "准备阶段失败！");
        return;
    }

    QByteArray standardVolumeFrame = QByteArray::fromHex("FD000601015B76355D ");
    serialPort->write(standardVolumeFrame);

    if (!waitForResponse(QByteArray::fromHex("4F"), 1000)) {
        QMessageBox::critical(this, "标准音量", "设置标准音量失败！");
    } else {
        QMessageBox::information(this, "标准音量", "设置标准音量成功！");
    }
}

void SerialSender::setMaxVolume() {
    if (!isSerialOpen) {
        QMessageBox::warning(this, "警告", "串口未打开！");
        return;
    }

    QByteArray prepareFrame = QByteArray::fromHex("FD000121");
    serialPort->write(prepareFrame);

    if (!waitForResponse(QByteArray::fromHex("4F"), 1000)) {
        QMessageBox::critical(this, "最大音量", "准备阶段失败！");
        return;
    }

    QByteArray maxVolumeFrame = QByteArray::fromHex("FD000706015B7631305D");
    serialPort->write(maxVolumeFrame);

    if (!waitForResponse(QByteArray::fromHex("4F"), 1000)) {
        QMessageBox::critical(this, "最大音量", "设置最大音量失败！");
    } else {
        QMessageBox::information(this, "最大音量", "设置最大音量成功！");
    }
}

void SerialSender::closeApplication() {
    if (isSerialOpen) {
        QMessageBox::warning(this, "警告", "请先关闭串口！");
        return;
    }

    QApplication::quit();
}

// void SerialSender::paintEvent(QPaintEvent *event) {
//     QPainter painter(this);

//     int x = portCombo->geometry().right() + 10;
//     int y = portCombo->geometry().top() + (portCombo->geometry().height() - 10) / 2;

//     painter.setBrush(isSerialOpen ? Qt::red : Qt::gray);
//     painter.setPen(Qt::NoPen);
//     painter.drawEllipse(x, y, 10, 10);
// }
