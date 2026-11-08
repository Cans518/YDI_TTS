#pragma once

#include "app/serialsender.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QPainter>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

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
    void initTestCase();
    void cleanup();
    void transactionRejectsInvalidSteps();
    void transactionDeviceLifetime();
    void progressCannotAcknowledgeUnsentFrame();
    void extremeParameterMapping();
    void sliderPressAndWideRange();
    void hiddenSourceDismissesHint();

    void protocolLength_data();
    void protocolLength();
    void encodingAndLimits();
    void voiceAndPitchProtocol();
    void configurationProtocolAndMapping();
    void configurationRepliesAreFramed();
    void asynchronousSequence();
    void timeoutAndWriteFailure();
    void cancelAndRestart();
    void reentrantProgressCancellation();
    void sliderBoundariesAndKeyboard();
    void sliderAnimationLifecycle();
    void buttonAndActivityMotion();
    void validationRecovery();
    void validationRecoveryAfterBusyOperation();
    void persistentHeaderLinks();
    void windowWorkflow();
    void windowTimeoutAndClose();
    void windowSizingAndEmptyPorts();
    void framelessDragSendAndClose();
    void soundSettingsWorkflow();
    void readConfigurationWorkflow();
    void startupIgnoresSavedPreferences();
    void failedSoundChangeKeepsConfirmedSettings();
    void styledHoverHints();
    void noticeCarouselRotatesAndPauses();
    void noticeCarouselPrioritiesAndPagination();
    void noticeCarouselIntegration();
};
