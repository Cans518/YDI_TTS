#ifndef NOTICECAROUSEL_H
#define NOTICECAROUSEL_H

#include <QLabel>
#include <QTimer>
#include <QVariantAnimation>

class NoticeCarousel : public QLabel
{
    Q_OBJECT
public:
    explicit NoticeCarousel(QWidget *parent = nullptr);
    void setMessage(const QString &message, const QString &tone = QStringLiteral("info"));
    void setTips(const QStringList &tips);
    void setRotationInterval(int milliseconds);
    int currentPage() const { return pageIndex; }
    int pageCount() const { return int(pages.size()); }

public slots:
    void next();

signals:
    void toneChanged(const QString &tone);
    void pageChanged(int index, int total);

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct Page
    {
        QString text;
        QString fullText;
        QString tone;
    };
    void rebuildPages(bool preservePage);
    void appendPages(const QString &message, const QString &tone, int width);
    void displayPage(int index, bool animate);
    void advance(int direction);
    void updateTimer();
    void settleAnimation();
    QRect textRect() const;

    QString message;
    QString messageTone = QStringLiteral("info");
    QString visibleTone;
    QStringList tips;
    QList<Page> pages;
    int pageIndex = 0;
    int interval = 4000;
    bool hovered = false;
    bool reflowPending = false;
    QTimer rotation;
    QVariantAnimation transition;
    QString outgoingText;
    QFont pageFont;
    qreal progress = 1;
};

#endif
