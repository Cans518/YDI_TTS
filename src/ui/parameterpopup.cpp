#include "ui/parameterpopup.h"
#include "core/ttsprotocol.h"
#include "ui/animatedbutton.h"
#include "ui/customlevelslider.h"
#include "ui/uitheme.h"

#include <QComboBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QScreen>

ParameterPopup::ParameterPopup(QWidget *parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setObjectName(QStringLiteral("soundPopup"));
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedWidth(320);
    setAccessibleName(QStringLiteral("音色与音调"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 15, 18, 15);
    layout->setSpacing(10);
    layout->setSizeConstraint(QLayout::SetDefaultConstraint);

    auto *heading = new QHBoxLayout;
    auto *title = new QLabel(accessibleName(), this);
    title->setObjectName(QStringLiteral("parameterTitle"));
    heading->addWidget(title);
    heading->addStretch();
    selectionLabel = new QLabel(this);
    selectionLabel->setObjectName(QStringLiteral("parameterValue"));
    heading->addWidget(selectionLabel);
    layout->addLayout(heading);

    auto *description = new QLabel(QStringLiteral("选择声音与高低，点击设置后一起生效。"), this);
    description->setObjectName(QStringLiteral("parameterDescription"));
    description->setWordWrap(true);
    layout->addWidget(description);
    auto *voiceRow = new QHBoxLayout;
    auto *voiceLabel = new QLabel(QStringLiteral("音色"), this);
    voiceLabel->setProperty("role", "fieldLabel");
    voiceChoice = new QComboBox(this);
    voiceChoice->setObjectName(QStringLiteral("voiceChoice"));
    voiceChoice->setAccessibleName(QStringLiteral("音色编号与类型"));
    voiceLabel->setBuddy(voiceChoice);
    for (int i = 0; i < int(TtsProtocol::VoiceIds.size()); ++i)
        voiceChoice->addItem(TtsProtocol::voiceLabel(i), TtsProtocol::VoiceIds[size_t(i)]);
    voiceRow->addWidget(voiceLabel);
    voiceRow->addWidget(voiceChoice, 1);
    layout->addLayout(voiceRow);

    auto *pitchRow = new QHBoxLayout;
    auto *pitchLabel = new QLabel(QStringLiteral("音调"), this);
    pitchLabel->setProperty("role", "fieldLabel");
    auto *low = new QLabel(QStringLiteral("低"), this);
    auto *high = new QLabel(QStringLiteral("高"), this);
    low->setObjectName(QStringLiteral("parameterDescription"));
    high->setObjectName(QStringLiteral("parameterDescription"));
    pitchSlider = new CustomLevelSlider(this);
    pitchSlider->setObjectName(QStringLiteral("pitchSlider"));
    pitchSlider->setAccessibleName(QStringLiteral("音调档位"));
    pitchSlider->setLevels(5);
    pitchSlider->setSliderHeight(36);
    pitchLabel->setBuddy(pitchSlider);
    connect(pitchSlider, &CustomLevelSlider::levelChanged, this, &ParameterPopup::updateSelectionLabel);
    pitchRow->addWidget(pitchLabel);
    pitchRow->addWidget(low);
    pitchRow->addWidget(pitchSlider, 1);
    pitchRow->addWidget(high);
    layout->addLayout(pitchRow);

    auto *actions = new QHBoxLayout;
    auto *cancel = new AnimatedButton(QStringLiteral("取消"), AnimatedButton::Tone::Quiet, this);
    cancel->setObjectName(QStringLiteral("cancelParameter"));
    cancel->setToolTip(QStringLiteral("收起面板，保留原来的设置。"));
    cancel->setProperty("hintShortcut", QStringLiteral("Esc"));
    connect(cancel, &QPushButton::clicked, this, &QWidget::hide);
    applyButton = new AnimatedButton(QStringLiteral("设置"), AnimatedButton::Tone::Primary, this);
    applyButton->setObjectName(QStringLiteral("applyParameter"));
    applyButton->setAccessibleName(QStringLiteral("设置音色和音调"));
    applyButton->setToolTip(QStringLiteral("应用所选参数，用于后续播报。"));
    connect(applyButton, &QPushButton::clicked, this, &ParameterPopup::applySelection);
    actions->addWidget(cancel);
    actions->addStretch();
    actions->addWidget(applyButton);
    layout->addLayout(actions);
    updateSelectionLabel();
}

int ParameterPopup::selectedVoiceId() const
{
    return voiceChoice->currentData().toInt();
}

int ParameterPopup::selectedPitchLevel() const
{
    return pitchSlider->currentLevel();
}

void ParameterPopup::updateSelectionLabel()
{
    selectionLabel->setText(QStringLiteral("音调 %1 / 5").arg(selectedPitchLevel() + 1));
}

void ParameterPopup::openAt(QWidget *anchor, int voiceId, int pitchValue)
{
    if (voiceChoice->count() > int(TtsProtocol::VoiceIds.size()))
        voiceChoice->removeItem(int(TtsProtocol::VoiceIds.size()));
    int index = voiceChoice->findData(voiceId);
    if (index < 0) {
        voiceChoice->addItem(TtsProtocol::voiceIdLabel(voiceId), voiceId);
        index = voiceChoice->count() - 1;
    }
    voiceChoice->setCurrentIndex(index);
    pitchSlider->setCurrentLevel(TtsProtocol::nearestLevel(pitchValue, TtsProtocol::PitchValues));
    pitchSlider->setToolTip(QStringLiteral("模块音调值：%1。五档对应 2、4、6、8、10。").arg(pitchValue));
    adjustSize();
    const auto *owner = anchor->window();
    const QPoint ownerTop = owner->mapToGlobal(QPoint(0, 0));
    auto *screen = QGuiApplication::screenAt(ownerTop + owner->rect().center());
    if (!screen)
        screen = anchor->screen();
    const QRect available = screen->availableGeometry().adjusted(6, 6, -6, -6);
    QPoint position(ownerTop.x() + (owner->width() - width()) / 2, ownerTop.y() + (owner->height() - height()) / 2);
    position.setX(qBound(available.left(), position.x(), qMax(available.left(), available.right() - width() + 1)));
    position.setY(qBound(available.top(), position.y(), qMax(available.top(), available.bottom() - height() + 1)));
    move(position);
    show();
    raise();
    voiceChoice->setFocus(Qt::PopupFocusReason);
}

void ParameterPopup::applySelection()
{
    const int voiceId = selectedVoiceId();
    const int pitchLevel = selectedPitchLevel();
    hide();
    emit applyRequested(voiceId, pitchLevel);
}

void ParameterPopup::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (auto *button = qobject_cast<QPushButton *>(focusWidget()))
            button->click();
        else
            applySelection();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void ParameterPopup::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    for (int spread = 3; spread >= 1; --spread) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(40, 57, 37, 10));
        p.drawRoundedRect(QRectF(rect()).adjusted(4 - spread, 5 - spread, -4 + spread, -3 + spread), 13, 13);
    }
    QLinearGradient gradient(0, 0, 0, height());
    gradient.setColorAt(0, UiTheme::Colors::Surface);
    gradient.setColorAt(1, UiTheme::Colors::SurfaceShade);
    p.setBrush(gradient);
    p.setPen(QPen(UiTheme::Colors::Border, 1));
    p.drawRoundedRect(QRectF(rect()).adjusted(3.5, 3.5, -3.5, -3.5), 11, 11);
}
