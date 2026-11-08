#ifndef SERIALTRANSACTION_H
#define SERIALTRANSACTION_H

#include <QElapsedTimer>
#include <QIODevice>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVector>

// One outstanding command at a time, with typed acknowledgement or parameter replies.
class SerialTransaction : public QObject
{
    Q_OBJECT
public:
    enum class Reply { Acknowledgement, Configuration };
    struct Step
    {
        QByteArray frame;
        QString description;
        Reply reply = Reply::Acknowledgement;
    };
    // Borrows the device. Destruction cancels any operation; start() then returns false.
    explicit SerialTransaction(QIODevice *device, QObject *parent = nullptr);
    // True means accepted; completion (including a synchronous write failure) uses finished().
    bool start(const QVector<Step> &steps, int timeoutMs = 1000);
    bool isBusy() const { return m_busy; }
    QByteArray response() const { return m_response; }
    void cancel(const QString &reason = QStringLiteral("操作已取消。"));

signals:
    void progress(const QString &description, int step, int total);
    void finished(bool success, const QString &message);

private:
    void writeNext();
    void receive();
    void completeStep();
    void finish(bool success, const QString &message = {});

    QPointer<QIODevice> m_device;
    QTimer m_deadline;
    QElapsedTimer m_frameGap;
    QVector<Step> m_steps;
    QByteArray m_received;
    QByteArray m_response;
    int m_index = 0;
    int m_timeoutMs = 1000;
    // Invalidates delayed callbacks when cancellation or a signal handler starts a new operation.
    quint64 m_generation = 0;
    bool m_busy = false;
    bool m_waiting = false;
};

#endif
