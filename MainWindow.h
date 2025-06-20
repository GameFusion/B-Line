
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

namespace Ui {
	class MainWindowBoarder;
}


class MainWindow : public QMainWindow
{
	Q_OBJECT


public:

	MainWindow(QWidget *parent = 0);
	virtual ~MainWindow();

    void updateShots();
    void updateScenes();
    void updateCharacters();

public slots:

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

protected:
	void dragEnterEvent(QDragEnterEvent *event) override;
	//void dragMoveEvent(QDragMoveEvent *event) override;
	//void dragLeaveEvent(QDragLeaveEvent *event) override;
	void dropEvent(QDropEvent *event) override;

    bool initializeLlamaClient();

protected:
	Ui::MainWindowBoarder *ui;

	GameFusion::Str tasks;
	GameFusion::GameScript script_tasks;
	GameFusion::GameTime animation_time;

    LlamaClient* llamaClient; // Externally managed LlamaClient
    GameFusion::ScriptBreakdown* scriptBreakdown; // Current script breakdown instance

    LlamaModel *llamaModel;
};



#endif

