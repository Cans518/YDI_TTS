#include "ui/levelsliderwidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>

LevelSliderWidget::LevelSliderWidget(QWidget *parent)
    : QWidget(parent)
{
    label = new QLabel(QStringLiteral("等级"), this);
    label->setProperty("role", "fieldLabel");
    label->setMinimumWidth(28);
    button = new AnimatedButton(QStringLiteral("设置"), AnimatedButton::Tone::Secondary, this);
    button->setObjectName(QStringLiteral("applyButton"));
    button->setMinimumWidth(72);
    connect(button, &QPushButton::clicked, this, &LevelSliderWidget::applyRequested);
    slider = new CustomLevelSlider(this);
    slider->setObjectName(QStringLiteral("levelSlider"));
    slider->setSliderHeight(28);
    label->setBuddy(slider);
    connect(slider, &CustomLevelSlider::levelChanged, this, &LevelSliderWidget::updateValueLabel);
    valueLabel = new QLabel(this);
    valueLabel->setProperty("role", "value");
    valueLabel->setAlignment(Qt::AlignCenter);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(label);
    layout->addWidget(slider, 1);
    layout->addWidget(valueLabel);
    layout->addWidget(button);
    setFixedHeight(28);
    updateValueLabel();
}

void LevelSliderWidget::setLabelText(const QString &text)
{
    label->setText(text);
    slider->setAccessibleName(text);
    button->setAccessibleName(button->text() + text);
    button->setToolTip(QStringLiteral("将选中的%1档位设置到模块。").arg(text));
}

void LevelSliderWidget::setDescription(const QString &text)
{
    slider->setToolTip(text);
}
int LevelSliderWidget::currentLevel() const
{
    return slider->currentLevel();
}
void LevelSliderWidget::setCurrentLevel(int level)
{
    slider->setCurrentLevel(level);
}
void LevelSliderWidget::setAppliedLevel(int level)
{
    appliedLevel = level;
    confirmedText = QStringLiteral("已设置");
    confirmedDescription = QStringLiteral("已设置到模块");
    updateValueLabel();
}
void LevelSliderWidget::setReadbackLevel(int level, int rawValue)
{
    setCurrentLevel(level);
    appliedLevel = level;
    confirmedText = QStringLiteral("已读取");
    confirmedDescription
        = QStringLiteral("模块%1值：%2，映射为第 %3 档。").arg(label->text()).arg(rawValue).arg(level + 1);
    updateValueLabel();
}
void LevelSliderWidget::setApplyEnabled(bool enabled)
{
    button->setEnabled(enabled);
}
void LevelSliderWidget::setButtonTone(AnimatedButton::Tone tone)
{
    button->setTone(tone);
}

void LevelSliderWidget::updateValueLabel()
{
    valueLabel->setText(QStringLiteral("%1 / 5").arg(currentLevel() + 1));
    const bool applied = currentLevel() == appliedLevel;
    valueLabel->setProperty("confirmed", applied);
    valueLabel->setToolTip(applied ? confirmedDescription : QStringLiteral("选择档位后点击设置"));
    button->setText(applied ? confirmedText : QStringLiteral("设置"));
    button->setAccessibleName(button->text() + label->text());
    valueLabel->style()->unpolish(valueLabel);
    valueLabel->style()->polish(valueLabel);
}
