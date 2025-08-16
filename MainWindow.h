
#ifndef MAINWINDOW_H
#define MAINWINDOW_H


#include <QMainWindow>
#include "ui_BoarderMainWindow.h"
#include "List.h"
#include "GameVar.h"
#include "GameScript.h"
#include "HashMap.h"
#include "GameTime.h"
#include "ScriptBreakdown.h"
#include "LlamaClient.h"
#include "StrokeAttributeDockWidget.h"

class LlamaModel;
class TimeLineView;
class MainWindowPaint;
class Segment;
class ShotPanelWidget;
class CameraSidePanel;
class PerfectScriptWidget;

namespace Ui {
	class MainWindowBoarder;
}

struct TimecodeDuration {
    qint64 durationMs=0;
    int frameCount=0;
};

struct PanelContext {
    GameFusion::Scene* scene = nullptr;
    GameFusion::Shot* shot = nullptr;
    GameFusion::Panel* panel = nullptr;

    bool isValid() const {
        return scene && shot && panel;
    }
};

struct LayerContext {
    GameFusion::Scene* scene = nullptr;
    GameFusion::Shot* shot = nullptr;
    GameFusion::Panel* panel = nullptr;
    GameFusion::Layer* layer = nullptr;

    bool isValid() {
        return scene && shot && panel && layer;
    }
};

struct ShotContext {
    GameFusion::Scene* scene = nullptr;
    GameFusion::Shot* shot = nullptr;

    bool isValid() const {
        return scene && shot;
    }
};

class MainWindow : public QMainWindow
{
	Q_OBJECT


public:

	MainWindow(QWidget *parent = 0);
	virtual ~MainWindow();

    void updateShots();
    void updateScenes();
    void updateCharacters();
    void loadProject(QString projectDir);
    void syncPanelDurations(Segment* segment, GameFusion::Shot* shot);

public slots:

    void newProject();
    void loadProject();
    void saveProject();
	void update();
	void postIssue();
	void teamEmail();
	void login();
	void quit();
	void about();

    void addAudioTrack();

	void setWhiteTheme();
	void setDarkTheme();
	void setBlackTheme();

    void importScript();
    void importAudioTrack();
    void importAudioSegment();

    void consoleCommandActivated(int index);
    bool consoleCommand(const QString &);

    void onTreeItemClicked(QTreeWidgetItem* item, int column);
    void onTimeCursorMoved(double time);

    void play();
    void pause();
    void stop();
    void nextShot();
    void nextScene();
    void prevShot();
    void prevScene();
    void onPlaybackTick();

    void onAddCamera();
    void onDuplicateCamera();
    void onDeleteCamera();
    void onRenameCamera();

    void onCameraFrameAddedFromPaint(const GameFusion::CameraFrame& frame);
    void onCameraFrameAddedFromSidePanel(const GameFusion::CameraFrame& frame);

    void onCameraFrameUpdated(const GameFusion::CameraFrame& frame);
    void onCameraFrameDeleted(const QString& uuid);

    void onLayerSelectionChanged(QListWidgetItem* current, QListWidgetItem* previous);
    void onLayerVisibilityChanged(QListWidgetItem* item);
    //void onLayerAdded(const GameFusion::Layer &layer);
    //void onLayerUpdated(const GameFusion::Layer &layer);
    //void onLayerDeleted(const QString &layerUuid);

    void onLayerBlendMode(int index);
    void onLayerLoadImage();
    void onLayerOpacity(int value);
    void onLayerFX();
    void onLayerAdd();
    void onLayerDelete();
    void onLayerMoveUp();
    void onLayerMoveDown();
    void onLayerReordered(const QModelIndex &parent,
                                      int start, int end,
                                      const QModelIndex &destination, int row);
    void onLayerRotationChanged(int value);
    void onLayerPosXChanged(double value);
    void onLayerPosYChanged(double value);
    void onLayerScaleChanged(double value);

    void onPaintAreaLayerAdded(const GameFusion::Layer& layer);
    void onPaintAreaLayerModified(const GameFusion::Layer& layer);

protected:
	void dragEnterEvent(QDragEnterEvent *event) override;
	//void dragMoveEvent(QDragMoveEvent *event) override;
	//void dragLeaveEvent(QDragLeaveEvent *event) override;
	void dropEvent(QDropEvent *event) override;

    bool initializeLlamaClient();
    void updateTimeline();
    ShotContext findShotByUuid(const std::string& uuid);
    PanelContext findPanelByUuid(const std::string& uuid);
    PanelContext findPanelForTime(double currentTime, double threshold = 0.05);
    ShotContext findShotForTime(double time/*, double buffer*/);
    LayerContext findLayerByUuid(const std::string& uuid);
    void populateLayerList(GameFusion::Panel* panel);

    QString generateUniqueShotName(GameFusion::Scene* scene);

    void addPanel(double t);
    void deletePanel(double t);
    void timelineOptions();

    void saveAudioTracks();
    void loadAudioTracks();
    void loadScript();

    void updateWindowTitle(bool isModified);

protected:
	Ui::MainWindowBoarder *ui;

	GameFusion::Str tasks;
	GameFusion::GameScript script_tasks;
	GameFusion::GameTime animation_time;

    LlamaClient* llamaClient; // Externally managed LlamaClient
    GameFusion::ScriptBreakdown* scriptBreakdown; // Current script breakdown instance

    LlamaModel *llamaModel;

    QString currentProjectName;
    QString currentProjectPath;

    TimeLineView* timeLineView;
    TimecodeDuration episodeDuration;

    MainWindowPaint *paint;

    GameFusion::Panel *currentPanel = nullptr;

    QJsonObject projectJson;

    ShotPanelWidget *shotPanel = nullptr;

    // Playback variables
    QTimer* playbackTimer = nullptr;
    bool isPlaying = false;
    bool loopEnabled = false;
    long playbackStart = 0;
    long playbackEnd = 0;
    long currentPlayTime = 0;
    int playbackIntervalMs = 33; // ~30 FPS

    // Camera side panel and attribute editor
    CameraSidePanel *cameraSidePanel = nullptr;

    //

    // The Script
    PerfectScriptWidget *perfectScript = nullptr;
};



#endif

