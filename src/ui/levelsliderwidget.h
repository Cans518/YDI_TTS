#ifndef LEVELSLIDERWIDGET_H
#define LEVELSLIDERWIDGET_H

#include "ui/animatedbutton.h"
#include "ui/customlevelslider.h"
#include <QWidget>

class QLabel;

class LevelSliderWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LevelSliderWidget(QWidget *parent = nullptr);
    void setLabelText(const QString &text);
    void setDescription(const QString &text);
    int currentLevel() const;
    void setCurrentLevel(int level);
    void setAppliedLevel(int level);
    void setReadbackLevel(int level, int rawValue);
    void setApplyEnabled(bool enabled);
    void setButtonTone(AnimatedButton::Tone tone);

signals:
    void applyRequested();

private:
    void updateValueLabel();
    QLabel *label;
    QLabel *valueLabel;
    AnimatedButton *button;
    CustomLevelSlider *slider;
    QString confirmedText = QStringLiteral("已设置");
    QString confirmedDescription = QStringLiteral("已设置到模块");
    int appliedLevel = -1;
};

#endif
