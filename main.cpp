
#include <QThread>
#include "MainWindow.h"

#include <QApplication>
#include <QSplashScreen>
#include <QTimer>
#include <QDir>
#include <QStandardPaths>

#include "Str.h"
#include "Log.h"
#include "Console.h"
#include "QtUtils.h"
#include "GameTime.h"
#include "SoundServer.h"
#include "GameCore.h"

#include "ShotPanelWidget.h"

#include "ConsoleDialog.h"

#ifdef ENABLE_PLUGIN_SUPPORT
#include <QtPlugin>
Q_IMPORT_PLUGIN(BasicToolsPlugin)
#endif

using GameFusion::Log;

static QString ResolveWritableLogPath()
{
    QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (basePath.isEmpty()) {
        basePath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath("B-Line");
    }

    QString logPath = QDir(basePath).filePath("logs");
    QDir dir;
    if (!dir.mkpath(logPath)) {
        logPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath("B-Line/logs");
        dir.mkpath(logPath);
    }

    if (!logPath.endsWith('/') && !logPath.endsWith('\\')) {
        logPath += '/';
    }

    return QDir::toNativeSeparators(logPath);
}

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Stargit Studio AB");
    QCoreApplication::setApplicationName("B-Line");
    QCoreApplication::setApplicationVersion(QStringLiteral(VERSION_STRING));

	// Prompt to attach debugger
#ifdef DEBUG
	MessageBox(NULL, L"Attach the debugger now (e.g., via Visual Studio 'Debug' -> 'Attach to Process').\nClick OK to continue.", L"Wait for Debug", MB_OK | MB_ICONINFORMATION);
#endif

    // Load splash screen image
    QPixmap pixmap(":/docs/logo-on-black.png");
    if (pixmap.isNull()) {
        qWarning() << "Failed to load splash screen image: docs/logo-on-black.png";
    }

    // Create splash screen
    QSplashScreen splash(pixmap);
    splash.setWindowFlag(Qt::WindowStaysOnTopHint);

    // Center on primary screen
    QRect screenGeometry = QGuiApplication::primaryScreen()->geometry();
    int x = (screenGeometry.width() - pixmap.width()) / 2;
    int y = (screenGeometry.height() - pixmap.height()) / 2;
    //splash.move(x, y);

    // Show splash screen
    splash.show();
    app.processEvents(); // Ensure splash screen is drawn immediately
    splash.showMessage("Loading...", Qt::AlignCenter, Qt::white);

    const QString logPathQt = ResolveWritableLogPath();
    const QByteArray logPathUtf8 = logPathQt.toUtf8();
    GameFusion::Str logPath(logPathUtf8.constData());
    printf("B-Line log path: %s\n", logPathUtf8.constData());
	GameFusion::Log::enableFile(logPath);
	GameFusion::Log::EnableQueue();
    GameFusion::Log::info("B-Line log path: %s\n", logPathUtf8.constData());


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
    const QByteArray appVersionUtf8 = appVersion.toUtf8();
    printf("B-Line version: %s\n", appVersionUtf8.constData());
    GameFusion::Log::info("B-Line version: %s\n", appVersionUtf8.constData());

	QFont f = app.font();
	//f.setPointSize(QFontDatabase::systemFont(QFontDatabase::FixedFont).pointSize() + 5);
	f.setPointSize(9);
	app.setFont(f);

    SetDarkXTheme();
	//SetWhiteTheme();
	//SetBlackTheme();

    //GameFusion::GameTime::usleep(2000);



    MainWindow w;

    // Close splash screen when main window is shown or after 3 seconds
    QTimer::singleShot(2000, &splash, &QSplashScreen::close);
    //QObject::connect(&w, &MainWindow::windowShown, &splash, &QSplashScreen::close);

    ConsoleDialog *console = new ConsoleDialog(&w);
    console->enableCommandPrompt(true);
    console->show();
    w.connect(console->inputBox(), &QComboBox::activated, &w, &MainWindow::consoleCommandActivated);

    w.showMaximized();

    //ShotPanelWidget sp;
    //sp.show();

    Log().info() << "Welcome to B-Line - Ai Powered Storyboarder\n";

    GameFusion::GameCore *core = new GameFusion::GameCore();
	GameFusion::SoundDevice::SampleRateDefault = 44100;
    core->initialize();
//    GameFusion::SoundServer::Initialize();

	return app.exec();
}
