#ifndef ACTIVITYINDICATOR_H
#define ACTIVITYINDICATOR_H

#include <QTimer>
#include <QWidget>

class ActivityIndicator : public QWidget
{
    Q_OBJECT
public:
    explicit ActivityIndicator(QWidget *parent = nullptr);
    void setState(const QString &state);

protected:
    void paintEvent(QPaintEvent *) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateTimer();
    QTimer m_timer;
    QString m_state;
    int m_angle = 0;
};

#endif
