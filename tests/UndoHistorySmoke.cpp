#include "MainWindow.h"
#include "MainWindowPaint.h"
#include "ProjectContext.h"
#include "../../TimeLineProject/TimeLineView.h"
#include "../../TimeLineProject/Track.h"
#include <QApplication>
#include <QDockWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>
#include <QUndoStack>
#include <QUndoView>
#include <cstdio>
#include <cstdlib>

static int checks = 0;
static void require(bool condition, const char *message)
{
    ++checks;
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

class HistoryFixture : public MainWindow {
public:
    void prepare(const QString &projectPath)
    {
        ProjectContext::instance().setCurrentProjectPath(projectPath);
        ProjectContext::instance().projectJson()["fps"] = 25;
        timeLineView->clear();
        timeLineView->addTrack(new Track("Shots", 0, 10000, TrackType::Storyboard));
        scriptBreakdown = new GameFusion::ScriptBreakdown("history-fixture", 25);
        GameFusion::Scene scene;
        scene.uuid = "history-scene";
        scene.name = "History fixture";
        GameFusion::Shot shot;
        shot.uuid = "history-shot";
        shot.name = "Drawing";
        shot.frameCount = 250;
        shot.startTime = 0;
        shot.endTime = 10000;
        GameFusion::Panel panel;
        panel.uuid = "history-panel";
        panel.name = "Panel 01";
        panel.startTime = 0;
        panel.durationTime = 10000;
        GameFusion::Layer layer;
        layer.uuid = "history-ink";
        layer.name = "Ink";
        panel.layers.push_back(layer);
        shot.panels.push_back(panel);
        scene.shots.push_back(shot);
        scriptBreakdown->getScenes().push_back(scene);
        updateScenes();
        updateTimeline();
        timeLineView->setTimeCursor(0L);
        currentPanel = &scriptBreakdown->getScenes().front().shots.front().panels.front();
        area()->setPanel(*currentPanel);
        area()->setActiveLayer("history-ink");
        area()->setToolMode(PaintArea::ToolMode::Paint);
        area()->setProjectPath(projectPath);
        populateLayerList(currentPanel);
        undoStack->clear();
    }
    PaintArea *area() { return paint->getPaintArea(); }
    QUndoStack *history() { return undoStack; }
    size_t strokes() const { return currentPanel->layers.front().strokes.size(); }
    int rotation() const { return currentPanel->layers.front().rotation; }
};

static void focusWindow(QWidget *window, QWidget *target)
{
    window->show();
    window->raise();
    window->activateWindow();
    require(QTest::qWaitForWindowActive(window), "test window activates for native shortcut dispatch");
    target->setFocus();
    QTest::qWait(30);
}

static void stroke(PaintCanvas *workspace, PaintArea *area, int y)
{
    auto mouse = [workspace](QEvent::Type type, QPointF scene, Qt::MouseButton button, Qt::MouseButtons buttons) {
        const QPointF pos = workspace->mapFromScene(scene);
        QMouseEvent event(type, pos, workspace->viewport()->mapToGlobal(pos.toPoint()), button, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(workspace->viewport(), &event);
    };
    mouse(QEvent::MouseButtonPress, {200.0, double(y)}, Qt::LeftButton, Qt::LeftButton);
    for (int i = 1; i <= 12; ++i)
        mouse(QEvent::MouseMove, {200.0 + i * 20, double(y + i * 3)}, Qt::NoButton, Qt::LeftButton);
    mouse(QEvent::MouseButtonRelease, {440.0, double(y + 36)}, Qt::LeftButton, Qt::NoButton);
    area->finishPendingStrokes();
    QTest::qWait(30);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settings.path());
    HistoryFixture window;
    window.prepare(settings.path());
    auto *workspace = window.findChild<PaintCanvas *>();
    auto *edit = window.findChild<QMenu *>("menuEdit");
    auto *undo = window.findChild<QAction *>("actionUndo");
    auto *redo = window.findChild<QAction *>("actionRedo");
    auto *historyAction = window.findChild<QAction *>("actionUndoHistory");
    auto *historyDock = window.findChild<QDockWidget *>("dockUndoHistory");
    auto *history = window.findChild<QUndoView *>("undoHistoryView");
    require(workspace && edit && undo && redo && historyAction && historyDock && history, "Edit menu and undo history controls exist");
    int editMenus = 0;
    for (auto *action : window.menuBar()->actions())
        if (action->text().remove('&') == "Edit") ++editMenus;
    require(editMenus == 1 && edit->actions().contains(undo) && edit->actions().contains(redo), "only one Edit menu contains Undo and Redo");
    require(edit->actions().contains(historyAction), "Undo History is available from Edit");
    require(!undo->isEnabled() && !redo->isEnabled(), "empty history disables Undo and Redo");
    require(history->model()->rowCount() == 1 && history->model()->index(0, 0).data().toString() == "Initial State", "empty history has a named initial state");

    workspace->resize(1000, 700);
    focusWindow(workspace, workspace);
    workspace->fitToBase();
    stroke(workspace, window.area(), 300);
    require(window.strokes() == 1 && window.history()->count() == 1, "pen stroke creates exactly one real undo command");
    require(undo->text().contains("Paint Stroke") && history->model()->index(1, 0).data().toString() == "Paint Stroke", "menu and history name the drawing action");
    QTest::keyClick(workspace, Qt::Key_Z, Qt::ControlModifier);
    require(window.strokes() == 0 && !undo->isEnabled() && redo->isEnabled(), "Ctrl+Z removes the stroke in the separate drawing workspace");
    QTest::keyClick(workspace, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    require(window.strokes() == 1 && !redo->isEnabled(), "Ctrl+Shift+Z restores the same stroke in the drawing workspace");

    workspace->hide();
    window.resize(1500, 950);
    focusWindow(&window, window.area());
    QTest::keyClick(window.area(), Qt::Key_Z, Qt::ControlModifier);
    require(window.strokes() == 0, "Ctrl+Z works in the integrated paint area");
    QTest::keyClick(window.area(), Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    require(window.strokes() == 1, "Ctrl+Shift+Z works in the integrated paint area");
#ifdef Q_OS_WIN
    QTest::keyClick(window.area(), Qt::Key_Z, Qt::ControlModifier);
    QTest::keyClick(window.area(), Qt::Key_Y, Qt::ControlModifier);
    require(window.strokes() == 1, "Windows Ctrl+Y remains a Redo shortcut");
#endif
    undo->trigger();
    require(window.strokes() == 0, "Edit Undo removes the stroke");
    redo->trigger();
    require(window.strokes() == 1, "Edit Redo restores the stroke");

    focusWindow(workspace, workspace);
    stroke(workspace, window.area(), 420);
    require(window.strokes() == 2 && window.history()->count() == 2, "successive strokes remain distinct history entries");
    workspace->hide();
    focusWindow(&window, window.area());
    auto *rotation = window.findChild<QSpinBox *>("spinBox_layerRotation");
    require(rotation != nullptr, "layer rotation control exists");
    rotation->setValue(20);
    require(window.rotation() == 20 && window.history()->count() == 3, "layer changes join the same undo history");
    require(history->model()->index(3, 0).data().toString() == "Change Layer Rotation", "non-paint actions have descriptive history names");
    rotation->setFocus();
    QTest::keyClick(rotation, Qt::Key_Z, Qt::ControlModifier);
    require(window.rotation() == 0, "Undo works from the layer control");
    QTest::keyClick(rotation, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    require(window.rotation() == 20, "Ctrl+Shift+Z works from the layer control");

    historyAction->trigger();
    QTest::qWait(80);
    require(historyDock->isVisible() && historyAction->isChecked(), "Edit Undo History reveals the dock");
    auto clickHistory = [history](int row) {
        const auto index = history->model()->index(row, 0);
        history->scrollTo(index);
        QTest::mouseClick(history->viewport(), Qt::LeftButton, Qt::NoModifier, history->visualRect(index).center());
    };
    clickHistory(1);
    require(window.strokes() == 1 && window.rotation() == 0 && window.history()->index() == 1, "selecting an earlier action undoes all later changes");
    clickHistory(3);
    require(window.strokes() == 2 && window.rotation() == 20 && window.history()->index() == 3, "selecting a later action redoes the intervening changes");
    clickHistory(0);
    require(window.strokes() == 0 && window.rotation() == 0, "Initial State undoes all recorded changes");
    clickHistory(1);
    historyDock->hide();
    require(!historyAction->isChecked(), "closing the history dock updates its menu toggle");
    focusWindow(workspace, workspace);
    stroke(workspace, window.area(), 530);
    require(window.strokes() == 2 && window.history()->count() == 2 && !redo->isEnabled(), "new drawing after Undo replaces the abandoned redo branch");
    require(history->model()->rowCount() == 3, "history list removes abandoned commands");
    window.history()->clear();
    require(history->model()->rowCount() == 1 && !undo->isEnabled() && !redo->isEnabled(), "clearing project history resets the list and both actions");
    printf("UndoHistorySmoke: PASS (%d checks: menus, real strokes, native shortcuts, named history navigation, branching)\n", checks);
    return 0;
}
