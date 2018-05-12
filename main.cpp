
#include <QThread>
#include "MainWindow.h"

#include <QApplication>
#include <QSplashScreen>

#include "Str.h"
#include "Log.h"
#include "Console.h"
#include "QtUtils.h"
#include "GameTime.h"

#include "ShotPanelWidget.h"

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	QPixmap pixmap("Btrfs3D.png");
	QSplashScreen splash(pixmap);
	splash.show();

	GameFusion::Str logPath("./");
	GameFusion::Log::enableFile(logPath);
	GameFusion::Log::EnableQueue();

#ifdef DEBUG
	GameFusion::Console::Show();
#endif

	for (int i = 1; i<argc; i++)
	{
		GameFusion::Str arg_str = argv[i];
		if (arg_str == "-client")
		{
			//GameFusion::server_mode = false;
			printf("client mode!\n");
		}
		else if (arg_str.match("-p", false))
		{
			GameFusion::Str port_str = arg_str.substring(2, arg_str.length() - 2);
			int port_number = port_str.atoi();
			//GameFusion::_port = port_number;
		}
		else if (arg_str.match("-s", false))
		{
			GameFusion::Str address_str = arg_str.substring(2, arg_str.length() - 2);
			//GameFusion::_address = address_str;
		}
	}


	QString appFilePath = QCoreApplication::applicationFilePath();
	QString appDirPath = QCoreApplication::applicationDirPath();
	QString appVersion = QCoreApplication::applicationVersion();

	QFont f = app.font();
	//f.setPointSize(QFontDatabase::systemFont(QFontDatabase::FixedFont).pointSize() + 5);
	f.setPointSize(9);
	app.setFont(f);

	SetDarkTheme();
	//SetWhiteTheme();
	//SetBlackTheme();

	GameFusion::GameTime::usleep(1000);



	MainWindow w;
	w.show();

	ShotPanelWidget sp;
	sp.show();

	return app.exec();
}

