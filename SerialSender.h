#ifndef SERIALSENDER_H
#define SERIALSENDER_H

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QFont>
#include <QIcon>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QPainter>
#include <QTimer>
#include <QElapsedTimer>
#include <QApplication>
#include <QDebug>
#include <QMouseEvent>

class SerialSender : public QWidget {
    Q_OBJECT

public:
    explicit SerialSender(QWidget *parent = nullptr);
    ~SerialSender();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private slots:
    void updatePorts();
    void toggleSerialPort();
    void sendData();
    void testConnection();
    void setDefaultSpeed();
    void setFastSpeed();
    void setStandardVolume();
    void setMaxVolume();
    void closeApplication();

private:
    QLabel *portLabel;
    QComboBox *portCombo;
    QLineEdit *textInput;
    QPushButton *sendButton;
    QPushButton *toggleButton;
    QPushButton *testButton;
    QPushButton *defaultSpeedButton;
    QPushButton *fastSpeedButton;
    QPushButton *standardVolumeButton;
    QPushButton *maxVolumeButton;
    QPushButton *closeButton;

    QSerialPort *serialPort;
    bool isSerialOpen;

    QPoint dragPosition; // 用于鼠标拖动窗口

    bool waitForResponse(const QByteArray &expectedData, int timeoutMs);
};

#endif // SERIALSENDER_H
