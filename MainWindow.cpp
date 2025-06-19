


#include "MainWindow.h"
//#include "Database.h"
#include "BezierPath.h"
#include "SceneNode.h"
#include "SceneGraph.h"
#include "AttributeEditor.h"

#include <QTimer>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QMessageBox>
#include <QFileDialog>

#include "QtUtils.h"

#include "ShotPanelWidget.h"
#include "MainWindowPaint.h"

#include "Log.h"

using namespace GameFusion;

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent), ui(new Ui::MainWindowBoarder)
{
	ui->setupUi(this);

    QAction *importScriptAction = ui->menuFile->addAction(tr("&Import Script"));
    connect(importScriptAction, &QAction::triggered, this, &MainWindow::importScript);

	QTimer *timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(update()));
	timer->start(1000);

	setAcceptDrops(true);

	QObject::connect(ui->actionPost_issue, SIGNAL(triggered()), this, SLOT(postIssue()));
	QObject::connect(ui->actionTeam_email, SIGNAL(triggered()), this, SLOT(teamEmail()));
	QObject::connect(ui->actionLogin, SIGNAL(triggered()), this, SLOT(login()));
	QObject::connect(ui->actionQuit, SIGNAL(triggered()), this, SLOT(quit()));

	QObject::connect(ui->actionWhite, SIGNAL(triggered()), this, SLOT(setWhiteTheme()));
	QObject::connect(ui->actionDark, SIGNAL(triggered()), this, SLOT(setDarkTheme()));
	QObject::connect(ui->actionBlack, SIGNAL(triggered()), this, SLOT(setBlackTheme()));

	connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(about()));

	ShotPanelWidget *shotPanel = new ShotPanelWidget;
	ui->splitter->insertWidget(0, shotPanel);
	shotPanel->show();

	MainWindowPaint *paint = new MainWindowPaint;
	ui->splitter->insertWidget(1, paint);
	paint->show();

	//
	// Test image in shot tree
	//new QTreeWidgetItem(shotsTreeWidget);
	QTreeWidget *shotTree = ui->shotsTreeWidget;
	shotTree->setIconSize(QSize(256, 256));

	QTreeWidgetItem *newItem = new QTreeWidgetItem(shotTree);
	newItem->setText(0, "Something");
	newItem->setIcon(1, QIcon("2019-01-05.png"));

	shotTree->topLevelItem(0)->setIcon(1, QIcon("2018-12-03 (1).png"));
	shotTree->topLevelItem(1)->setIcon(1, QIcon("2018-12-03.png"));
	shotTree->topLevelItem(2)->setIcon(1, QIcon("2018-12-15.png"));
	shotTree->topLevelItem(3)->setIcon(1, QIcon("2019-01-05 (5).png"));
	newItem = new QTreeWidgetItem(shotTree);
	newItem->setText(0, "SH100");
	newItem->setIcon(1, QIcon("2018-09-15 (6).png"));
	newItem = new QTreeWidgetItem(shotTree);
	newItem->setText(0, "SH101");
	newItem->setIcon(1, QIcon("2018-09-12 (3).png"));
	newItem = new QTreeWidgetItem(shotTree);
	newItem->setText(0, "SH102");
	newItem->setIcon(1, QIcon("2018-09-15 (4).png"));
	//newItem->setSizeHint(1, QSize(256, 144));
	
	
	//shotTree->addTopLeveItem(newItem);
	
	//
	//// RETURN

	return;
	//
	QList <int> list;
	list += 150;
	list += 500;
	list += 500;

	ui->splitter->setSizes(list);
}


void MainWindow::setWhiteTheme()
{
	printf("set to white\n");
	SetWhiteTheme();
}

void MainWindow::setDarkTheme()
{
	SetDarkTheme();
}

void MainWindow::setBlackTheme()
{
	SetBlackTheme();
}
MainWindow::~MainWindow()
{
	
}


void MainWindow::quit()
{
	exit(0);
}

void MainWindow::postIssue()
{
	
}

void MainWindow::teamEmail()
{

}

void MainWindow::login()
{

}

void MainWindow::update()
{
	static int update_count(0);
	printf("update %d\n", update_count);
	update_count++;

}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
	printf("dragEnter\n");

	//if (event->mimeData()->hasFormat("text/plain"))
	event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
	printf("Got dropEvent!\n");

	//
	//-----------------------------------------------
	//

	GameFusion::Str plainText = (char*)event->mimeData()->text().constData();
	printf("drop mime data = '%s'\n", (char*)plainText);

	const QMimeData* mimeData = event->mimeData();

	{
		QByteArray data = mimeData->data("FileNameW");
		QString filename = QString::fromUtf16((ushort*)data.data(), data.size() / 2);
		Str tmp = (char*)filename.toLatin1().constData();
		printf("**** fileNameW = %s\n", (char*)tmp);
	}

	// check for our needed mime type, here a file or a list of files
	if (mimeData->hasUrls())
	{
		QStringList pathList;
		QList<QUrl> urlList = mimeData->urls();

		// extract the local paths of the files
		for (int i = 0; i < urlList.size(); i++)
		{
			QString file = urlList.at(i).toLocalFile();
			Str file_str = (char*)file.toLatin1().constData();
			pathList.append(urlList.at(i).toLocalFile());
			printf("local path for file %d : %s\n", i, (char*)file_str);
		}
	}

	{

		QList<QUrl> urls = event->mimeData()->urls();

		for (int i = 0; i < urls.length(); i++)
		{
			printf("url %d\n", i);
			GameFusion::Str urlText = (char*)urls[i].fileName().toLatin1().constData();;
			printf("urlText '%s'\n", (char*)urlText);
		}
	}

	//textBrowser->setPlainText(event->mimeData()->text());
	//mimeTypeCombo->clear();
	//mimeTypeCombo->addItems(event->mimeData()->formats());

	event->acceptProposedAction();
}


void MainWindow::about()
{
	int gmajor(1);
	int gminor(0);
	int rev(1);

	GameFusion::Str title;
	title.sprintf("DropPublish 2018 (v %d.%d)", gmajor, gminor);
	printf("DropPublish version : major %d minor %d rev %d", gmajor, gminor, rev);

	GameFusion::Str message;

	message += "Layout (c) 2018 - GAME FUSION - Andreas Carlen\n\n";
	//message += "Build SHA1 ID #173178c796e76ada82b749070314fc0dcd121f76\n\n";
	//
	message += GameFusion::Str().sprintf("Using OpenSSL, Qt 5.6 and GameFusion API", gmajor, gminor, rev);
	QMessageBox::about(0, "About", (char*)message);


}

bool MainWindow::initializeLlamaClient(const QString& modelPath, const QString& backend)
{
    if (llamaClient) {
        return true; // Already initialized
    }

#ifdef _WIN32
    llamaClient = LlamaClient::Create(backend.toStdString(), "LlamaEngine.dll");
#elif __APPLE__
    llamaClient = LlamaClient::Create(backend.toStdString(), "LlamaEngine.dylib");
#endif
    if (!llamaClient) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to create LlamaClient: %1").arg(QString::fromStdString(LlamaClient::GetCreateError())));
        return false;
    }

    // Define parameter values
    float temperature = 0.7f;
    int max_tokens = 512;

    // Initialize ModelParameter array
    ModelParameter params[] = {
        {"temperature", PARAM_FLOAT, &temperature},
        {"max_tokens", PARAM_INT, &max_tokens}
    };

    if (!llamaClient->loadModel(modelPath.toStdString(), params, 2, [](const char* msg) {
            qDebug() << "Model load:" << msg;
        })) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to load Llama model: %1").arg(modelPath));
        return false;
    }

    return llamaClient->createSession(1);
}

void MainWindow::importScript()
{
    // Open file dialog to select .fdx file
    QString fileName = QFileDialog::getOpenFileName(this, tr("Import Script"), "", tr("Final Draft Files (*.fdx)"));
    if (fileName.isEmpty()) {
        return; // User canceled
    }

    // Initialize LlamaClient if not already done (adjust model path as needed)
    if (!initializeLlamaClient("/Users/andreascarlen/Library/Application Support/StarGit/models/Qwen2.5-7B-Instruct-1M-Q4_K_M.gguf", "Metal")) {
        return;
    }

    // Create or update ScriptBreakdown instance
    delete scriptBreakdown; // Clean up previous instance

    GameScript* dictionary = NULL;
    GameScript* dictionaryCustom = NULL;

    scriptBreakdown = new ScriptBreakdown(fileName.toStdString().c_str(), dictionary, dictionaryCustom, llamaClient);

    // Perform breakdown (use ChunkedContext mode and enable sequences for testing)
    if (!scriptBreakdown->breakdownScript(llamaClient, "/Users/andreascarlen/Library/Application Support/StarGit/models/Qwen2.5-7B-Instruct-1M-Q4_K_M.gguf", "Metal",
                                          GameFusion::ScriptBreakdown::BreakdownMode::ChunkedContext, true)) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to process script: %1").arg(fileName));
        delete scriptBreakdown;
        scriptBreakdown = nullptr;
        return;
    }

    // Clear existing tree items
    ui->shotsTreeWidget->clear();

    // Populate shotsTreeWidget with sequences, scenes, and shots
    const auto& sequences = scriptBreakdown->getSequences();
    if (!sequences.empty()) {
        for (const auto& seq : sequences) {
            QTreeWidgetItem* seqItem = new QTreeWidgetItem(ui->shotsTreeWidget);
            seqItem->setText(0, QString::fromStdString(seq.name));
            seqItem->setIcon(1, QIcon("sequence_icon.png")); // Placeholder icon
            for (const auto& scene : seq.scenes) {
                QTreeWidgetItem* sceneItem = new QTreeWidgetItem(seqItem);
                sceneItem->setText(0, QString::fromStdString(scene.name));
                sceneItem->setIcon(1, QIcon("scene_icon.png")); // Placeholder icon
                for (const auto& shot : scene.shots) {
                    QTreeWidgetItem* shotItem = new QTreeWidgetItem(sceneItem);
                    shotItem->setText(0, QString::fromStdString(shot.name));
                    shotItem->setIcon(1, QIcon("shot_icon.png")); // Placeholder icon
                    shotItem->setData(0, Qt::UserRole, QVariant::fromValue(&shot)); // Store shot data
                }
            }
        }
    } else {
        // Fallback to scenes if sequences are disabled
        const auto& scenes = scriptBreakdown->getScenes();
        for (const auto& scene : scenes) {
            QTreeWidgetItem* sceneItem = new QTreeWidgetItem(ui->shotsTreeWidget);
            sceneItem->setText(0, QString::fromStdString(scene.name));
            sceneItem->setIcon(1, QIcon("scene_icon.png")); // Placeholder icon
            for (const auto& shot : scene.shots) {
                QTreeWidgetItem* shotItem = new QTreeWidgetItem(sceneItem);
                shotItem->setText(0, QString::fromStdString(shot.name));
                shotItem->setIcon(1, QIcon("shot_icon.png")); // Placeholder icon
                shotItem->setData(0, Qt::UserRole, QVariant::fromValue(&shot)); // Store shot data
            }
        }
    }

    // Update ShotPanelWidget (pseudo-code, adjust based on ShotPanelWidget API)
    ShotPanelWidget* shotPanel = ui->splitter->findChild<ShotPanelWidget*>();
    if (shotPanel) {
        // Clear existing panels
        // shotPanel->clearPanels();
        // for (const auto& scene : scriptBreakdown->getScenes()) {
        //     for (const auto& shot : scene.shots) {
        //         shotPanel->addShot({
        //             .name = QString::fromStdString(shot.name),
        //             .type = QString::fromStdString(shot.type),
        //             .description = QString::fromStdString(shot.description),
        //             .frameCount = shot.frameCount,
        //             .dialogue = QString::fromStdString(shot.dialogue),
        //             .fx = QString::fromStdString(shot.fx),
        //             .notes = QString::fromStdString(shot.notes)
        //         });
        //     }
        // }
    }

    QMessageBox::information(this, tr("Success"), tr("Script imported successfully: %1").arg(fileName));
}

void MainWindow::consoleCommandActivated(int index)
{
    QComboBox *comboBox = qobject_cast<QComboBox *>(sender());
    if (!comboBox) {
        qWarning("consoleCommandActivated: sender is not a QComboBox!");
        return;
    }

    QString commandText = comboBox->itemText(index);
    if(consoleCommand(commandText))
        comboBox->setCurrentText("");
}

bool MainWindow::consoleCommand(const QString &the_command_line)
{
    Log().info() << the_command_line.toUtf8().constData() << "\n";

    return true;
}
