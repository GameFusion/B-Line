
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

class LlamaModel;
class TimeLineView;
class MainWindowPaint;

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

	void setWhiteTheme();
	void setDarkTheme();
	void setBlackTheme();

    void importScript();

    void consoleCommandActivated(int index);
    bool consoleCommand(const QString &);

    void onTreeItemClicked(QTreeWidgetItem* item, int column);
    void onTimeCursorMoved(double time);

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

    QString generateUniqueShotName(GameFusion::Scene* scene);

    void addPanel(double t);
    void deletePanel(double t);

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
};



#endif

