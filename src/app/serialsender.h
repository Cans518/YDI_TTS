#ifndef SERIALSENDER_H
#define SERIALSENDER_H

#include "core/serialtransaction.h"
#include "core/ttsprotocol.h"
#include <QPixmap>
#include <QWidget>

class ActivityIndicator;
class AnimatedButton;
class LevelSliderWidget;
class ParameterPopup;
class NoticeCarousel;
class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QSerialPort;
class QTimer;

class SerialSender : public QWidget
{
    Q_OBJECT
public:
    explicit SerialSender(QWidget *parent = nullptr);
    // Takes ownership; allows transport integration tests without physical hardware.
    SerialSender(QSerialPort *serialPort, QWidget *parent);
    ~SerialSender() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void updatePorts();
    void toggleSerialPort();
    void sendData();
    void testConnection();
    void applyVolumeLevel();
    void applySpeedLevel();
    void applySound(int voiceId, int pitchLevel);
    void readConfiguration();

private:
    enum class Operation { None, Speech, Test, Volume, Speed, Sound, ReadConfiguration };
    void buildUi();
    void updateControls();
    void updateTextState();
    void disconnectPort();
    void showFeedback(const QString &message, const QString &tone = QStringLiteral("info"));
    void beginOperation(Operation operation, const QVector<SerialTransaction::Step> &steps, const QString &success,
        int timeoutMs = 1000);
    QVector<SerialTransaction::Step> initializationSteps() const;

    QComboBox *portCombo = nullptr;
    QLineEdit *textInput = nullptr;
    AnimatedButton *sendButton = nullptr;
    AnimatedButton *toggleButton = nullptr;
    AnimatedButton *testButton = nullptr;
    AnimatedButton *refreshButton = nullptr;
    AnimatedButton *closeButton = nullptr;
    AnimatedButton *soundButton = nullptr;
    AnimatedButton *readButton = nullptr;
    ParameterPopup *soundPopup = nullptr;
    QLabel *connectionBadge = nullptr;
    QLabel *byteCount = nullptr;
    QFrame *toolPanel = nullptr;
    QFrame *notice = nullptr;
    NoticeCarousel *noticeText = nullptr;
    ActivityIndicator *indicator = nullptr;
    LevelSliderWidget *volumeSliderWidget = nullptr;
    LevelSliderWidget *speedSliderWidget = nullptr;
    QSerialPort *serialPort;
    SerialTransaction *transaction;
    QTimer *serialMonitorTimer;
    QStringList currentPortSignature;
    QString preferredPort;
    QString successMessage;
    Operation currentOperation = Operation::None;
    int submittedLevel = 0;
    TtsProtocol::Configuration currentConfiguration;
    TtsProtocol::Configuration submittedConfiguration;
    bool connected = false;
    bool disconnecting = false;
    bool validText = false;
    QPixmap background;
    QPixmap backgroundCache;
    QPixmap frostedBackgroundCache;
    QSize backgroundCacheSize;
    QPoint dragOffset;
    bool dragging = false;
};

#endif
