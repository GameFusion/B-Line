
#include "ShotPanelWidget.h"

ShotPanelWidget::ShotPanelWidget(QWidget *parent)
		: QWidget(parent), ui(new Ui::ShotPanelWidget)
{
	ui->setupUi(this);
}
	
ShotPanelWidget::~ShotPanelWidget()
{
		;
}