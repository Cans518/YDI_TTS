#ifndef CUSTOMLEVELSLIDER_H
#define CUSTOMLEVELSLIDER_H

#include <QSlider>
#include <QVariantAnimation>

// QSlider retains native keyboard and accessibility value semantics.
class CustomLevelSlider : public QSlider
{
    Q_OBJECT
public:
    explicit CustomLevelSlider(QWidget *parent = nullptr);
    void setLevels(int count);
    void setCurrentLevel(int level) { setValue(level); }
    int currentLevel() const { return value(); }
    void setSliderHeight(int height);
    QSize sizeHint() const override { return QSize(260, height()); }
    QSize minimumSizeHint() const override { return QSize(80, height()); }

signals:
    void levelChanged(int level);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    qreal targetPosition() const;
    QRectF trackRect() const;
    void updateLevelFromPosition(const QPointF &pos);
    void animateToValue();
    void settleAnimation();

    QVariantAnimation m_animation;
    qreal m_position = 0;
};

#endif
