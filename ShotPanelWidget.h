
#include "ui_ShotPanelWidget.h"

class ShotPanelWidget : public QWidget
{
	Q_OBJECT


public:

	ShotPanelWidget(QWidget *parent = 0);
	virtual ~ShotPanelWidget();

protected:
	Ui::ShotPanelWidget *ui;
};

