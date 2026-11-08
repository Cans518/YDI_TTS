#include "ttstests.h"
#include "ui/activityindicator.h"
#include "ui/animatedbutton.h"
#include "ui/customlevelslider.h"
#include "ui/noticecarousel.h"
#include "ui/uitheme.h"
#include <QAccessible>
#include <QEnterEvent>
#include <QHelpEvent>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <limits>

void TtsTests::sliderBoundariesAndKeyboard()
{
    CustomLevelSlider slider;
    slider.resize(280, 44);
    slider.setAccessibleName(QStringLiteral("音量"));
    slider.show();
    slider.setFocus();
    QSignalSpy changed(&slider, &CustomLevelSlider::levelChanged);
    QTest::keyClick(&slider, Qt::Key_End);
    QCOMPARE(slider.currentLevel(), 4);
    QCOMPARE(changed.count(), 1);
    QTest::keyClick(&slider, Qt::Key_Left);
    QCOMPARE(slider.currentLevel(), 3);
    slider.setLevels(1);
    QCOMPARE(slider.currentLevel(), 0);
    slider.resize(20, 44);
    QTest::mouseClick(&slider, Qt::LeftButton, Qt::NoModifier, QPoint(10, 20));
    QCOMPARE(slider.currentLevel(), 0);
    QVERIFY(!slider.grab().isNull());
    slider.setLevels(5);
    slider.resize(280, 44);
    UiTheme::setReducedMotion(true);
    slider.setCurrentLevel(4);
    QCoreApplication::processEvents();
    const QImage atEnd = slider.grab().toImage();
    QTest::qWait(30);
    QCOMPARE(slider.grab().toImage(), atEnd);
    auto *accessible = QAccessible::queryAccessibleInterface(&slider);
    QVERIFY(accessible);
    QCOMPARE(accessible->role(), QAccessible::Slider);
    QVERIFY(accessible->valueInterface());
    QCOMPARE(accessible->valueInterface()->currentValue().toInt(), 4);
}

void TtsTests::sliderAnimationLifecycle()
{
    CustomLevelSlider slider;
    slider.resize(300, 44);
    slider.show();
    QCoreApplication::processEvents();
    const QImage initial = slider.grab().toImage();
    slider.setCurrentLevel(4);
    QCOMPARE(slider.currentLevel(), 4); // Value changes before the visual transition finishes.
    const QImage starting = slider.grab().toImage();
    QTest::qWait(220);
    const QImage settled = slider.grab().toImage();
    QVERIFY(settled != initial);
    QVERIFY(settled != starting);
    slider.setCurrentLevel(0);
    slider.hide();
    slider.show();
    const QImage afterShow = slider.grab().toImage();
    QTest::qWait(200);
    QCOMPARE(slider.grab().toImage(), afterShow);
}

void TtsTests::buttonAndActivityMotion()
{
    AnimatedButton button(QStringLiteral("发送"));
    button.resize(90, 34);
    button.show();
    QCoreApplication::processEvents();
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(&button, &leave);
    QTest::qWait(180);
    const QImage normal = button.grab().toImage();
    QEnterEvent enter(QPointF(8, 8), QPointF(8, 8), QPointF(8, 8));
    QApplication::sendEvent(&button, &enter);
    QTest::qWait(180);
    QVERIFY(button.grab().toImage() != normal);
    UiTheme::setReducedMotion(true);
    QApplication::sendEvent(&button, &leave);
    QCOMPARE(button.grab().toImage(), normal);

    UiTheme::setReducedMotion(false);
    ActivityIndicator activity;
    activity.show();
    activity.activateWindow();
    QCoreApplication::processEvents();
    auto *ticker = activity.findChild<QTimer *>();
    QVERIFY(ticker);
    activity.setState(QStringLiteral("busy"));
    QCOMPARE(ticker->isActive(), qApp->applicationState() == Qt::ApplicationActive);
    activity.hide();
    QVERIFY(!ticker->isActive());
    activity.show();
    UiTheme::setReducedMotion(true);
    QVERIFY(!ticker->isActive());
    UiTheme::setReducedMotion(false);
    activity.setState(QStringLiteral("success"));
    QVERIFY(!ticker->isActive());
}

void TtsTests::styledHoverHints()
{
    SerialSender window;
    window.findChild<QTimer *>(QStringLiteral("portMonitor"))->stop();
    window.show();
    QCoreApplication::processEvents();
    auto *button = child<AnimatedButton>(window, "refreshButton");
    const QPoint local = button->rect().center();
    QHelpEvent help(QEvent::ToolTip, local, button->mapToGlobal(local));
    QApplication::sendEvent(button, &help);
    QTRY_VERIFY(visibleHint());
    auto *hint = visibleHint();
    QCOMPARE(hint->focusPolicy(), Qt::NoFocus);
    QCOMPARE(hint->findChild<QLabel *>(QStringLiteral("hintShortcut"))->text(), QStringLiteral("F5"));
    QCOMPARE(hint->findChild<QLabel *>(QStringLiteral("hintBody"))->text(), button->toolTip());
    QTest::qWait(160);
    QVERIFY(captureOverlay(window, *hint, "11-hover-hint"));
    QVERIFY(capture(*hint, "17-hover-hint-detail"));
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(button, &leave);
    QVERIFY(!visibleHint());

    QHelpEvent cornerHelp(QEvent::ToolTip, local, button->screen()->availableGeometry().bottomRight() - QPoint(2, 2));
    QApplication::sendEvent(button, &cornerHelp);
    QTRY_VERIFY(visibleHint());
    QVERIFY(visibleHint()->screen()->availableGeometry().contains(visibleHint()->geometry()));
    button->setToolTip(QStringLiteral("新的提示文字"));
    QVERIFY(!visibleHint());
    QApplication::sendEvent(button, &help);
    QTRY_VERIFY(visibleHint());
    QTest::mouseClick(button, Qt::LeftButton);
    QVERIFY(!visibleHint());
    QApplication::sendEvent(button, &help);
    QTRY_VERIFY(visibleHint());
    window.close();
    QVERIFY(!visibleHint());
    window.show();
    auto *temporaryButton = new AnimatedButton(QStringLiteral("临时按钮"), AnimatedButton::Tone::Secondary, &window);
    temporaryButton->setToolTip(QStringLiteral("控件销毁后应立即收起"));
    temporaryButton->show();
    QHelpEvent temporaryHelp(QEvent::ToolTip, QPoint(5, 5), temporaryButton->mapToGlobal(QPoint(5, 5)));
    QApplication::sendEvent(temporaryButton, &temporaryHelp);
    QTRY_VERIFY(visibleHint());
    delete temporaryButton;
    QVERIFY(!visibleHint());
}

void TtsTests::noticeCarouselRotatesAndPauses()
{
    QWidget host;
    auto *layout = new QVBoxLayout(&host);
    auto *focusTarget = new QLineEdit(&host);
    auto *carousel = new NoticeCarousel(&host);
    layout->addWidget(focusTarget);
    layout->addWidget(carousel);
    host.resize(340, 100);
    carousel->setTips({ QStringLiteral("第二条提示"), QStringLiteral("第三条提示") });
    carousel->setMessage(QStringLiteral("第一条提示"));
    carousel->setRotationInterval(70);
    host.show();
    host.activateWindow();
    focusTarget->setFocus();
    QCoreApplication::processEvents();
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(carousel, &leave);
    QTRY_VERIFY(carousel->currentPage() != 0);

    QEnterEvent enter(QPointF(5, 5), QPointF(5, 5), carousel->mapToGlobal(QPoint(5, 5)));
    QApplication::sendEvent(carousel, &enter);
    const auto page = carousel->currentPage();
    const auto tooltip = carousel->toolTip();
    QTest::qWait(320);
    QCOMPARE(carousel->currentPage(), page);
    QCOMPARE(carousel->toolTip(), tooltip);
    carousel->setMessage(QStringLiteral("设置完成"), QStringLiteral("success"));
    QCOMPARE(carousel->currentPage(), 0);
    QCOMPARE(carousel->text(), QStringLiteral("设置完成"));
    QTest::qWait(100);
    QCOMPARE(carousel->currentPage(), 0);
    QApplication::sendEvent(carousel, &leave);
    QTRY_VERIFY(carousel->currentPage() != 0);

    host.hide();
    const int hiddenPage = carousel->currentPage();
    QVERIFY(!carousel->findChild<QTimer *>(QStringLiteral("noticeRotation"))->isActive());
    QTest::qWait(180);
    QCOMPARE(carousel->currentPage(), hiddenPage);
}

void TtsTests::noticeCarouselPrioritiesAndPagination()
{
    NoticeCarousel carousel;
    carousel.setFocusPolicy(Qt::NoFocus);
    carousel.resize(220, 26);
    carousel.setTips({ QStringLiteral("普通操作提示"), QStringLiteral("快捷键说明") });
    carousel.show();
    carousel.setMessage(QStringLiteral("正在发送文本"), QStringLiteral("busy"));
    QCOMPARE(carousel.pageCount(), 1);
    carousel.next();
    QCOMPARE(carousel.text(), QStringLiteral("正在发送文本"));
    QVERIFY(!carousel.findChild<QTimer *>(QStringLiteral("noticeRotation"))->isActive());

    const QString fullError = QStringLiteral(
        "设备连接已经中断，请检查模块供电、串口选择和数据线连接，确认设备就绪以后重新打开串口再发送播报文本。");
    QSignalSpy tones(&carousel, &NoticeCarousel::toneChanged);
    carousel.setMessage(fullError, QStringLiteral("error"));
    QVERIFY(carousel.pageCount() > 1);
    const int narrowCount = carousel.pageCount();
    QString reconstructed;
    for (int i = 0; i < narrowCount; ++i) {
        reconstructed += carousel.text();
        QCOMPARE(carousel.toolTip(), fullError);
        QCOMPARE(carousel.accessibleDescription(), fullError);
        carousel.next();
    }
    QCOMPARE(reconstructed, fullError);
    QCOMPARE(carousel.currentPage(), 0);
    QCOMPARE(tones.count(), 1); // Critical pages never switch to unrelated tips.
    QCOMPARE(tones.first().first().toString(), QStringLiteral("error"));
    carousel.resize(360, 26);
    QVERIFY(carousel.pageCount() < narrowCount);
    QCOMPARE(carousel.toolTip(), fullError);

    const QString english
        = QStringLiteral("Please check the USB cable and reopen the serial port before sending another message.");
    carousel.resize(220, 26);
    carousel.setMessage(english, QStringLiteral("error"));
    QString reconstructedEnglish;
    for (int i = 0; i < carousel.pageCount(); ++i) {
        reconstructedEnglish += carousel.text();
        carousel.next();
    }
    QCOMPARE(reconstructedEnglish, english);

    const QString multiline = QStringLiteral("串口断开\n请检查连接");
    carousel.resize(carousel.fontMetrics().horizontalAdvance(multiline) + 1, 26);
    carousel.setMessage(multiline, QStringLiteral("error"));
    for (int i = 0; i < carousel.pageCount(); ++i) {
        const int available = carousel.contentsRect().width() - (carousel.pageCount() > 1 ? 34 : 0);
        QVERIFY(carousel.fontMetrics().horizontalAdvance(carousel.text()) <= available);
        QCOMPARE(carousel.toolTip(), multiline);
        carousel.next();
    }
    carousel.resize(360, 26);

    carousel.setMessage(QStringLiteral("操作已完成"), QStringLiteral("success"));
    QCOMPARE(carousel.currentPage(), 0);
    QCOMPARE(carousel.text(), QStringLiteral("操作已完成"));
    QCOMPARE(carousel.pageCount(), 3);
    QTest::mouseClick(&carousel, Qt::LeftButton);
    QCOMPARE(carousel.currentPage(), 1);
    QTest::keyClick(&carousel, Qt::Key_Left);
    QCOMPARE(carousel.currentPage(), 0);
}

void TtsTests::sliderPressAndWideRange()
{
    CustomLevelSlider slider;
    slider.resize(280, 44);
    slider.show();
    QCoreApplication::processEvents();
    slider.setValue(4); // Interrupt an animation by pressing its existing target value.
    QTest::mousePress(&slider, Qt::LeftButton, Qt::NoModifier, QPoint(260, 22));
    QCOMPARE(slider.value(), 4);
    const QImage pressed = slider.grab().toImage();
    QTest::qWait(200);
    QCOMPARE(slider.grab().toImage(), pressed); // No delayed animation during a held press.
    QTest::mouseRelease(&slider, Qt::LeftButton, Qt::NoModifier, QPoint(260, 22));
    slider.setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    QTest::mouseClick(&slider, Qt::LeftButton, Qt::NoModifier, QPoint(0, 22));
    QCOMPARE(slider.value(), std::numeric_limits<int>::min());
    QTest::mouseClick(&slider, Qt::LeftButton, Qt::NoModifier, QPoint(279, 22));
    QCOMPARE(slider.value(), std::numeric_limits<int>::max());
    QVERIFY(!slider.grab().isNull());
}

void TtsTests::hiddenSourceDismissesHint()
{
    QWidget window;
    AnimatedButton button("hint", AnimatedButton::Tone::Quiet, &window);
    button.setToolTip("hide with the source");
    window.show();
    QCoreApplication::processEvents();
    QHelpEvent help(QEvent::ToolTip, QPoint(5, 5), button.mapToGlobal(QPoint(5, 5)));
    QApplication::sendEvent(&button, &help);
    QTRY_VERIFY(visibleHint());
    button.hide();
    QVERIFY(!visibleHint());
}
