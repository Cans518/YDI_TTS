#ifndef PARAMETERPOPUP_H
#define PARAMETERPOPUP_H

#include <QWidget>

class AnimatedButton;
class CustomLevelSlider;
class QComboBox;
class QLabel;

class ParameterPopup : public QWidget
{
    Q_OBJECT
public:
    explicit ParameterPopup(QWidget *parent = nullptr);
    void openAt(QWidget *anchor, int voiceId, int pitchValue);
    int selectedVoiceId() const;
    int selectedPitchLevel() const;

signals:
    void applyRequested(int voiceId, int pitchLevel);

protected:
    void paintEvent(QPaintEvent *) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void applySelection();
    void updateSelectionLabel();
    QComboBox *voiceChoice = nullptr;
    CustomLevelSlider *pitchSlider = nullptr;
    QLabel *selectionLabel;
    AnimatedButton *applyButton;
};

#endif
