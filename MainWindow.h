
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
    GameFusion::Shot* findShotByUuid(const std::string& uuid);
    GameFusion::Panel* findPanelByUuid(const std::string& uuid);

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

