#ifndef ANIMATEDBUTTON_H
#define ANIMATEDBUTTON_H

#include <QPushButton>
#include <QVariantAnimation>

class AnimatedButton : public QPushButton
{
    Q_OBJECT
public:
    enum class Tone { Primary, Secondary, Quiet, Success, Warning, Danger, Volume, Speed };
    explicit AnimatedButton(const QString &text, Tone tone = Tone::Secondary, QWidget *parent = nullptr);
    void setTone(Tone tone);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void animateHover(qreal target);
    Tone m_tone;
    QVariantAnimation m_hoverAnimation;
    qreal m_hover = 0;
};

#endif
