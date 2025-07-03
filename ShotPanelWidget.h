
#include "ui_ShotPanelWidget.h"

#include "ScriptBreakdown.h"

class ShotPanelWidget : public QWidget
{
	Q_OBJECT


public:

	ShotPanelWidget(QWidget *parent = 0);
	virtual ~ShotPanelWidget();

    void setPanelInfo(GameFusion::Scene *scene, GameFusion::Shot *shot, GameFusion::Panel *panel, float fps);

protected:
	Ui::ShotPanelWidget *ui;
};

