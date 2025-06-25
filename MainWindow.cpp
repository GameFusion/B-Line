


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
#include <QThread>

#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>


#include "QtUtils.h"

#include "ShotPanelWidget.h"
#include "MainWindowPaint.h"
#include "LlamaModel.h"
#include "BreakdownWorker.h"
#include "ErrorDialog.h"

#include "Log.h"

// BEGIN TimeLine related includes
#include <QCheckBox>
#include <QScrollBar>
#include "../TimeLineProject/TimeLineWidget.h"
#include "../TimeLineProject/TimeLineView.h"
#include "../TimeLineProject/CustomSlider.h"
#include "../TimeLineProject/ScrollbarView.h"
#include "../TimeLineProject/Track.h"
#include "../TimeLineProject/Segment.h"
#include "../TimeLineProject/MarkerItem.h"
#include "../TimeLineProject/ShotSegment.h"
#include "../TimeLineProject/PanelMarker.h"
#include "../TimeLineProject/CursorItem.h"
// END TimeLine related includes

using namespace GameFusion;


//#include "TimeLineWidget.h"


TimeLineView* createTimeLine(QWidget &mainWindow)
{
    mainWindow.setWindowTitle("Timeline Viewer");

    ///
    int fontId = QFontDatabase::addApplicationFont(":/fa-regular-400.ttf");
    printf("fond Id %d\n", fontId);

    QString fontFamily = "Helvetica";
    if (fontId == -1) {
        qWarning("Failed to load FontAwesome font");
    }
    else
        fontFamily = QFontDatabase::applicationFontFamilies(fontId).at(0);

    QFont fontAwesome(fontFamily);

    // Create buttons
    QToolButton *shiftLeftButton = new QToolButton(&mainWindow);
    shiftLeftButton->setText(QChar(0xf061));  // Shift left icon
    shiftLeftButton->setFont(fontAwesome);

    QToolButton *shiftRightButton = new QToolButton(&mainWindow);
    shiftRightButton->setText(QChar(0xf060));  // Group selection icon
    shiftRightButton->setFont(fontAwesome);

    QToolButton *phonemeButton = new QToolButton(&mainWindow);
    phonemeButton->setText(QChar(0xf044));  // Single selection icon
    phonemeButton->setFont(fontAwesome);

    QToolButton *editButton = new QToolButton(&mainWindow);
    editButton->setText(QChar(0xf044));  // Area selection icon
    editButton->setFont(fontAwesome);

    ///

    // Create and configure the TimeLineView
    //CustomGraphicsScene *scene = new CustomGraphicsScene();
    TimeLineView *timelineView = new TimeLineView(&mainWindow);

    //timelineView->setScene(scene);
    QGraphicsScene *scene = timelineView->scene();
    timelineView->setMinimumSize(80, 60); // Set a minimum size for the timeline view
    //timelineView->scale(0.5, 1);

    // Create a QSpinBox for scale control
    QSpinBox *scaleControl = new QSpinBox(&mainWindow);
    scaleControl->setFixedWidth(100);
    scaleControl->setRange(10, 200); // Set range from 1x to 10x
    scaleControl->setValue(100); // Initial scale factor set to 1
    scaleControl->setPrefix("Scale: ");
    //scaleControl->setSuffix("x");

    // Create a QSlider for scale control
    //QSlider *scaleSlider = new QSlider(Qt::Horizontal, &mainWindow);
    CustomSlider *scaleSlider = new CustomSlider(Qt::Horizontal, &mainWindow);
    scaleSlider->setRange(10, 200); // Matching range with the spin box
    scaleSlider->setValue(100); // Initial value matching the spin box
    scaleSlider->setFixedWidth(400);

    // Create a new QGraphicsView as a custom scrollbar
    ScrollbarView *scrollbarView = new ScrollbarView(&mainWindow);
    scrollbarView->setFixedHeight(28);  // Set the height of the scrollbar
    scrollbarView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Connect the scale control to the view scaling functionality
    QObject::connect(scaleControl, QOverload<int>::of(&QSpinBox::valueChanged),
                     [timelineView, scaleSlider](int scaleValue) {
                         // Adjust only the x-scale based on the spin box value
                         //qreal currentYScale = timelineView->transform().m22(); // Get the current y-scale
                         //timelineView->setTransform(QTransform::fromScale(100.f/scaleValue, currentYScale));
                         // update the timeline
                         timelineView->setUiScale(scaleValue/100.f);

                         bool oldState = scaleSlider->blockSignals(true);
                         scaleSlider->setValue(scaleValue);
                         scaleSlider->blockSignals(oldState);
                     });

    //connect(scaleSlider, &CustomSlider::prepareForScaling, this, &YourClass::handlePrepareForScaling);

    QObject::connect(scaleSlider, &CustomSlider::prepareForScaling, [timelineView]() {
        // Capture the center point when the user starts to change the scale
        printf("prepare For Sliding\n");
        fflush(stdout);
        QPointF focus = timelineView->getCenterPoint();
        timelineView->setZoomFocus(focus);
    });

    QObject::connect(scaleSlider, &QSlider::valueChanged, [timelineView, scaleControl](int value) {
        //qreal scaleY = timelineView->transform().m22(); // Maintain current Y scale
        //timelineView->setTransform(QTransform::fromScale(value / 100.0, scaleY));
        printf("valueChanged For Sliding\n");
        fflush(stdout);

        scaleControl->setValue(value); // Update spin box when slider changes
        bool oldState = scaleControl->blockSignals(true);
        scaleControl->setValue(value);
        scaleControl->blockSignals(oldState);

        // update the timeline
        timelineView->setUiScale(value/100.f);
    });

    QObject::connect(timelineView, &TimeLineView::updateZoom,
                     [scaleSlider](double newZoomLevel) {
                         int sliderValue = static_cast<int>(newZoomLevel * 100);  // Convert zoom level to slider value
                         scaleSlider->updateValue(sliderValue);
                     });

    QObject::connect(timelineView, &TimeLineView::updateZoom,
                     [scaleControl](double newZoomLevel) {
                         int spinBoxValue = static_cast<int>(newZoomLevel * 100);  // Convert zoom level to spin box value
                         bool oldState = scaleControl->blockSignals(true);
                         scaleControl->setValue(spinBoxValue);
                         scaleControl->blockSignals(oldState);
                     });


    timelineView->scene()->setSceneRect(0, 0, 20000, 700);  // Adjust dimensions as needed
    timelineView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);


    // Create tracks
    Track *track1 = new Track("Track 1", 0, 500000); // Name, Start Time, Duration in seconds
    Track *track2 = new Track("Track 2", 0, 150000); // Name, Start Time, Duration in seconds
    Track *track3 = new Track("Track 3", 0, 170000); // Name, Start Time, Duration in seconds

    // Add tracks to the timeline
    TrackItem *t1 = timelineView->addTrack(track1);
    TrackItem *t2 = timelineView->addTrack(track2);
    //timelineView->addTrack(track3);

    Segment* segment = new Segment(scene, 100, 400);
    t1->addSegment(segment);
    //t1->addSegment(5, 100);
    // Layout setup (if necessary)

    //scene->addItem(t1);

    new MarkerItem(20, 30, 20, 220, segment, "Hello", "HH", "(OS)");
    new MarkerItem(40, 30, 40, 220, segment, "", "EH", "");
    new MarkerItem(70, 30, 70, 220, segment, "", "L", "");
    new MarkerItem(90, 30, 90, 220, segment, "", "OW", "");
    new MarkerItem(110, 30, 110, 220, segment, "", "*", "");

    new MarkerItem(120, 30, 10, 220, segment, "World", "W", "");
    new MarkerItem(140, 30, 140, 220, segment, "", "ER", "");
    new MarkerItem(170, 30, 170, 220, segment, "", "L", "");
    new MarkerItem(190, 30, 190, 220, segment, "", "D", "");
    new MarkerItem(225, 30, 225, 220, segment, "<SIL>", "", "");
    //layout->addStretch(1); // Adds a spacer to push the timeline to the left




    Segment* segment2 = new Segment(scene, 200, 500);
    t2->addSegment(segment2);
    new PanelMarker(0, 30, 0, 220, segment2, "Panel 1", "HH", "(OS)");
    new PanelMarker(140, 30, 140, 220, segment2, "Panel 2", "OW", "WIDE");





    ShotSegment* segment3 = new ShotSegment(scene, 600, 500);
    t2->addSegment(segment3);
    //new MarkerItem(70, 30, 70, 220, segment3, "", "L", "");



    // Layout setup
    /*
    QHBoxLayout *layout = new QHBoxLayout(&mainWindow); // Use QHBoxLayout for horizontal layout
    layout->addWidget(timelineView);
    layout->addStretch(1); // Adds a spacer to push the timeline to the left
*/

    // Create a QComboBox for visual style selection
    QComboBox *styleComboBox = new QComboBox(&mainWindow);
    styleComboBox->addItem("Grayscale");
    styleComboBox->addItem("Inferno");
    styleComboBox->addItem("Viridis");
    styleComboBox->addItem("Cividis");
    styleComboBox->addItem("Plasma");
    styleComboBox->addItem("Turbo");
    styleComboBox->addItem("Roseus");

    styleComboBox->addItem("None");

    // Create a checkbox for waveform display
    QCheckBox *waveformCheckbox = new QCheckBox("Waveform", &mainWindow);
    waveformCheckbox->setChecked(false);

    QSpacerItem *spacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    QVBoxLayout *toplayout = new QVBoxLayout(&mainWindow);
    QHBoxLayout *hlayout = new QHBoxLayout(&mainWindow);

    QVBoxLayout *gfxlayout = new QVBoxLayout(&mainWindow);

    gfxlayout->setSpacing(0);  // Remove spacing between the elements

    hlayout->addWidget(scaleControl); // Add the scale control at the top
    hlayout->addWidget(scaleSlider); // Add the scale control at the top
    hlayout->addWidget(styleComboBox);  // Add the style combobox
    hlayout->addWidget(waveformCheckbox);
    hlayout->addWidget(shiftLeftButton);
    hlayout->addWidget(shiftRightButton);
    hlayout->addWidget(phonemeButton);
    hlayout->addWidget(editButton);
    //hlayout->addWidget(singleSelectionButton);
    //hlayout->addWidget(areaSelectionButton);

    hlayout->addSpacerItem(spacer);
    gfxlayout->addWidget(timelineView);
    gfxlayout->addWidget(scrollbarView);

    toplayout->addLayout(hlayout);
    toplayout->addLayout(gfxlayout);


    mainWindow.setLayout(toplayout);

    // Connect ScrollbarView to TimelineView's scrollbar
    QObject::connect(scrollbarView, &ScrollbarView::valueChanged,
                     timelineView->horizontalScrollBar(), &QScrollBar::setValue);
    /*
    // Connect TimelineView's scrollbar to ScrollbarView
    QObject::connect(timelineView->horizontalScrollBar(), &QScrollBar::valueChanged,
                     scrollbarView, &ScrollbarView::setValue);
*/
    // Setup connections
    QObject::connect(timelineView, &TimeLineView::updateScroll,
                     scrollbarView, &ScrollbarView::setValue);

    // Connect the visual style combo box to the timeline view
    QObject::connect(styleComboBox, &QComboBox::currentTextChanged,
                     timelineView, &TimeLineView::setVisualStyle);

    QObject::connect(waveformCheckbox, &QCheckBox::stateChanged, timelineView, &TimeLineView::toggleWaveformDisplay);


    scrollbarView->setRange(0,20000);

    // Show the main window
    mainWindow.show();

    t1->loadAudio("WarnerTest", "/users/andreascarlen/GameFusion/GameEngine/Applications/GameEditor/WarnerTest/TT.257.500.ACT.B.TEST44100.wav");
    t2->loadAudio("WarnerTest", "/users/andreascarlen/GameFusion/GameEngine/Applications/GameEditor/WarnerTest/TT.257.500.ACT.B.TEST44100.wav");

    return timelineView;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindowBoarder), llamaModel(nullptr), scriptBreakdown(nullptr)
{
	ui->setupUi(this);

    QAction *importScriptAction = ui->menuFile->addAction(tr("&Import Script"));
    connect(importScriptAction, &QAction::triggered, this, &MainWindow::importScript);

	QTimer *timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(update()));
	timer->start(1000);

	setAcceptDrops(true);

    QObject::connect(ui->actionOpen, SIGNAL(triggered()), this, SLOT(loadProject()));
	QObject::connect(ui->actionPost_issue, SIGNAL(triggered()), this, SLOT(postIssue()));
	QObject::connect(ui->actionTeam_email, SIGNAL(triggered()), this, SLOT(teamEmail()));
	QObject::connect(ui->actionLogin, SIGNAL(triggered()), this, SLOT(login()));
	QObject::connect(ui->actionQuit, SIGNAL(triggered()), this, SLOT(quit()));

	QObject::connect(ui->actionWhite, SIGNAL(triggered()), this, SLOT(setWhiteTheme()));
	QObject::connect(ui->actionDark, SIGNAL(triggered()), this, SLOT(setDarkTheme()));
	QObject::connect(ui->actionBlack, SIGNAL(triggered()), this, SLOT(setBlackTheme()));

	connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(about()));
/*
	ShotPanelWidget *shotPanel = new ShotPanelWidget;
	ui->splitter->insertWidget(0, shotPanel);
	shotPanel->show();
*/

    ShotPanelWidget *shotPanel = new ShotPanelWidget;
    shotPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred); // allow vertical growth

    QScrollArea *scrollArea = new QScrollArea;
    scrollArea->setWidget(shotPanel);
    scrollArea->setWidgetResizable(true); // ensures resizing works correctly
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    ui->splitter->insertWidget(0, scrollArea);
    scrollArea->show();

    /***/

    paint = new MainWindowPaint;
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

    timeLineView = createTimeLine(*ui->timeline);

    // Add Callback for Tree Item Selection
    connect(ui->shotsTreeWidget, &QTreeWidget::itemClicked, this, &MainWindow::onTreeItemClicked);


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

#include <QPointer>
static QPointer<MainWindow> g_mainWindowInstance = nullptr;

bool MainWindow::initializeLlamaClient()
{
    QString modelPath ="/Users/andreascarlen/Library/Application Support/StarGit/models/Qwen2.5-7B-Instruct-1M-Q4_K_M.gguf";
    QString backend = "Metal";
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
    int max_tokens = 4096*4;

    // Initialize ModelParameter array
    ModelParameter params[] = {
        {"temperature", PARAM_FLOAT, &temperature},
        {"context_size", PARAM_INT, &max_tokens},
        {"max_tokens", PARAM_INT, &max_tokens}
    };

    // Set global GUI-safe pointer
    g_mainWindowInstance = this;

    if (!llamaClient->loadModel(modelPath.toStdString(), params, 2, [](const char* msg) {
            qDebug() << "Model load:" << msg;

            Log().info() << msg;
            if(Str(msg).startsWith("[ERROR]")){
                Log().error() << msg;

                // Call GUI code from main thread
                QString message = QString::fromUtf8(msg);
                QMetaObject::invokeMethod(g_mainWindowInstance, [message]() {
                    QMessageBox::critical(g_mainWindowInstance, QObject::tr("Llama Engine Error"), message);
                }, Qt::QueuedConnection);
            }
        })) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to load Llama model: %1").arg(modelPath));
        delete llamaClient;
        llamaClient=nullptr;
        return false;
    }

    llamaModel = new LlamaModel(nullptr, llamaClient);

    return llamaClient->createSession(1);
}

void addShot(QTreeWidgetItem* shotItem, const GameFusion::Shot &shot) {

    QString cameraText = QString::fromStdString(shot.camera.movement + " / " + shot.camera.framing);
    QString audioText = QString::fromStdString(shot.audio.ambient);
    if (!shot.audio.sfx.empty()) {
        audioText += " | SFX: ";
        for (size_t i = 0; i < shot.audio.sfx.size(); ++i) {
            audioText += QString::fromStdString(shot.audio.sfx[i]);
            if (i < shot.audio.sfx.size() - 1) audioText += ", ";
        }
    }

    /*
    QString characterDialogSummary;
    for (const auto& c : shot.characters) {
        QString line = QString::fromStdString(c.name);
        if (!c.dialogue.empty()) {
            line += ": ";
            if (!c.dialogParenthetical.empty())
                line += "(" + QString::fromStdString(c.dialogParenthetical) + ") ";
            line += QString::fromStdString(c.dialogue);
        }
        characterDialogSummary += line + "\n";
    }
    characterDialogSummary = characterDialogSummary.trimmed();
    */

    shotItem->setText(0, QString::fromStdString(shot.name));
    shotItem->setText(1, QString::fromStdString(shot.type));
    shotItem->setText(2, QString::fromStdString(shot.transition));
    shotItem->setText(3, cameraText);
    shotItem->setText(4, QString::fromStdString(shot.lighting));
    shotItem->setText(5, audioText);
    shotItem->setText(6, QString::fromStdString(shot.description));
    shotItem->setText(7, QString::number(shot.frameCount));
    shotItem->setText(8, QString::fromStdString(shot.timeOfDay));
    shotItem->setText(9, shot.restore ? "true" : "false");
    shotItem->setText(10, QString::fromStdString(shot.fx));
    shotItem->setText(11, QString::fromStdString(shot.notes));
    shotItem->setText(12, QString::fromStdString(shot.intent));
    //shotItem->setText(13, characterDialogSummary);

    if(!shot.characters.empty()){
        for (const auto& c : shot.characters) {
            auto* charItem = new QTreeWidgetItem(shotItem);
            charItem->setText(0, QString::fromStdString(c.name));
            charItem->setText(1, QString::number(c.dialogNumber));
            charItem->setText(2, QString::fromStdString(c.emotion));
            charItem->setText(3, QString::fromStdString(c.intent));
            charItem->setText(4, c.onScreen ? "on screen" : "off screen");
            charItem->setText(5, QString::fromStdString(c.dialogParenthetical));
            charItem->setText(6, QString::fromStdString(c.dialogue));
        }
    }

    if (!shot.panels.empty()) {
        for (const auto& panel : shot.panels) {
            QTreeWidgetItem* panelItem = new QTreeWidgetItem(shotItem);
            panelItem->setText(0, QString::fromStdString(panel.name));
            panelItem->setText(1, QString::fromStdString(panel.description));
            panelItem->setText(2, QString::number(panel.startFrame));
            panelItem->setText(3, QString::number(panel.durationFrames));
            panelItem->setText(4, QString::fromStdString(panel.uuid));

            // Store UUID or full Panel pointer (if lifetime is stable)
            panelItem->setData(0, Qt::UserRole, QString::fromStdString(panel.uuid));
        }
    }
}

void MainWindow::importScript()
{

    // Open file dialog to select .fdx file
    QString fileName = QFileDialog::getOpenFileName(this, tr("Import Script"), "", tr("Final Draft Files (*.fdx)"));
    if (fileName.isEmpty()) {
        return; // User canceled
    }

    // Initialize LlamaClient if not already done (adjust model path as needed)
    if (!initializeLlamaClient()) {
        return;
    }

    // Create or update ScriptBreakdown instance
    if(scriptBreakdown)
        delete scriptBreakdown; // Clean up previous instance

    GameScript* dictionary = NULL;
    GameScript* dictionaryCustom = NULL;

    scriptBreakdown = new ScriptBreakdown(fileName.toStdString().c_str(), dictionary, dictionaryCustom, llamaClient);


    // Threaded breakdown using QThread
    QThread* thread = new QThread;
    BreakdownWorker* worker = new BreakdownWorker(scriptBreakdown);

    worker->moveToThread(thread);

    // Connect signals
    connect(thread, &QThread::started, worker, &BreakdownWorker::process);
    connect(worker, &BreakdownWorker::finished, this, [=](bool success) {
        thread->quit();
        thread->wait();
        worker->deleteLater();
        thread->deleteLater();

        if (!success) {
            QMessageBox::critical(this, tr("Error"), tr("Failed to process script: %1").arg(fileName));
            delete scriptBreakdown;
            scriptBreakdown = nullptr;
            return;
        }

        // Reuse your existing UI update code here ↓
        ui->shotsTreeWidget->clear();

        //const auto& shots = scriptBreakdown->getShots();

        const auto& sequences = scriptBreakdown->getSequences();
        if (!sequences.empty()) {
            for (const auto& seq : sequences) {
                QTreeWidgetItem* seqItem = new QTreeWidgetItem(ui->shotsTreeWidget);


                seqItem->setText(0, QString::fromStdString(seq.name));
                seqItem->setIcon(1, QIcon("sequence_icon.png"));
                for (const auto& scene : seq.scenes) {
                    QTreeWidgetItem* sceneItem = new QTreeWidgetItem(seqItem);

                    sceneItem->setText(0, QString::fromStdString(scene.name));
                    sceneItem->setIcon(1, QIcon("scene_icon.png"));
                    for (const auto& shot : scene.shots) {
                        QTreeWidgetItem* shotItem = new QTreeWidgetItem(sceneItem);
                        addShot(shotItem, shot);

                    }
                }
           }
        } else {
            const auto& scenes = scriptBreakdown->getScenes();
            for (const auto& scene : scenes) {
                QTreeWidgetItem* sceneItem = new QTreeWidgetItem(ui->shotsTreeWidget);
                sceneItem->setText(0, QString::fromStdString(scene.name));
                sceneItem->setIcon(1, QIcon("scene_icon.png"));
                for (const auto& shot : scene.shots) {
                    QTreeWidgetItem* shotItem = new QTreeWidgetItem(sceneItem);
                    addShot(shotItem, shot);
                }
            }
        }

        ui->shotsTreeWidget->expandAll();

        QMessageBox::information(this, tr("Success"), tr("Script imported successfully: %1").arg(fileName));
    });

    connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    // Optional per-shot signal
    /*
    connect(worker, &BreakdownWorker::shotCreated, this, [=](const QString& sequence, const QString& scene, const QString& shotName) {
        // Implement UI progressive update here if breakdownScript supports emitting per shot
    });
    */
    thread->start();
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



void MainWindow::updateShots(){
    if(!scriptBreakdown) return;

    ui->shotsTreeWidget->clear();

    const auto& shots = scriptBreakdown->getShots();
    for (const auto& shot : shots) {
        QTreeWidgetItem* shotItem = new QTreeWidgetItem(ui->shotsTreeWidget);
        addShot(shotItem, shot);
    }

    ui->shotsTreeWidget->expandAll();
}

void MainWindow::updateScenes(){
    if(!scriptBreakdown) return;

    ui->shotsTreeWidget->clear();

    const auto& scenes = scriptBreakdown->getScenes();
    for (const auto& scene : scenes) {
        QTreeWidgetItem* sceneItem = new QTreeWidgetItem(ui->shotsTreeWidget);
        sceneItem->setText(0, QString::fromStdString(scene.name));
        sceneItem->setIcon(1, QIcon("scene_icon.png"));
        for (const auto& shot : scene.shots) {
            QTreeWidgetItem* shotItem = new QTreeWidgetItem(sceneItem);
            addShot(shotItem, shot);
        }
    }

    ui->shotsTreeWidget->expandAll();
}

void MainWindow::updateTimeline(){

    const auto& scenes = scriptBreakdown->getScenes();

    episodeDuration.durationMs = 0;
    episodeDuration.frameCount = 0;



    TrackItem * track = timeLineView->getTrack(0);
    QGraphicsScene *gfxscene = timeLineView->scene();

    //gfxscene->clear();

    qreal fps = 25;
    qreal mspf = 1000.f/fps;
    long startTimeFrame = episodeDuration.frameCount;

    for (const auto& scene : scenes) {

        Log().info() << "Scene Start Frame: " << scene.name.c_str() << " " << startTimeFrame << "\n";

        timeLineView->addSceneMarker(startTimeFrame * mspf, scene.name.c_str());

        for (const auto& shot : scene.shots) {

            long shotFrameStart = startTimeFrame;  // place shot at current frame
            qreal shotTimeStart = shotFrameStart * mspf;
            qreal shotDuration = shot.frameCount * mspf;

            ShotSegment* segment = new ShotSegment(gfxscene, shotTimeStart, shotDuration);

            Log().info() << "  Shot Start Frame: " << shot.name.c_str() << " " << shotFrameStart
                         << " ms start: " << (float)shotTimeStart << "\n";

            segment->marker()->setShotLabel(shot.name.c_str());
            segment->marker()->setPanelName(shot.name.c_str());

            int pannelIndex(0);

            for (const auto& panel : shot.panels) {
                long panelFrameStart = startTimeFrame + panel.startFrame;
                qreal panelTimeStart = panelFrameStart * mspf;
                qreal panelDuration = panel.durationFrames * mspf;
                QString thumbnail = currentProjectPath + "/movies/" + panel.thumbnail.c_str();

                // by default ShotSegment had one initial panel
                PanelMarker *panelMarker = pannelIndex == 0 ? segment->marker() : new PanelMarker(140, 30, 140, 220, segment, panel.name.c_str(), "", "");

                panelMarker->setPanelName(QString::fromStdString(panel.name));
                panelMarker->loadThumbnail(thumbnail);

                pannelIndex ++;
            }

            track->addSegment(segment);

            // Advance frame cursor by shot's frame count
            startTimeFrame += shot.frameCount;
        }

        // After each scene, update the global episode duration
        episodeDuration.frameCount = startTimeFrame;
        episodeDuration.durationMs = startTimeFrame * mspf;
    }
}

void MainWindow::updateCharacters(){
    if(!scriptBreakdown) return;

    ui->shotsTreeWidget->clear();

    const auto& characters = scriptBreakdown->getCharacters();
    for (const auto& character : characters) {
        QTreeWidgetItem* characterItem = new QTreeWidgetItem(ui->shotsTreeWidget);

        characterItem->setText(0, QString::fromStdString(character.name));                          // Column 0: Name
        characterItem->setText(1, QString::fromStdString(character.emotion));                      // Column 1: Emotion
        characterItem->setText(2, QString::fromStdString(character.intent));                       // Column 2: Intent
        characterItem->setText(3, character.onScreen ? "true" : "false");                          // Column 3: OnScreen
        characterItem->setText(4, QString::number(character.dialogNumber));                        // Column 4: Dialogue #
        characterItem->setText(5, QString::fromStdString(character.dialogParenthetical));          // Column 5: Parenthetical
        characterItem->setText(6, QString::fromStdString(character.dialogue));                     // Column 6: Line
    }

    ui->shotsTreeWidget->expandAll();
}

bool MainWindow::consoleCommand(const QString &the_command_line)
{
    Log().info() << the_command_line.toUtf8().constData() << "\n";

    if(the_command_line == "printShots"){
        if(scriptBreakdown)
            scriptBreakdown->printShots();
        return true;
    }

    if(the_command_line == "printScenes"){
        if(scriptBreakdown)
            scriptBreakdown->printScenes();
        return true;
    }

    if(the_command_line == "printCharacters"){
        if(scriptBreakdown)
            scriptBreakdown->printCharacters();
        return true;
    }

    if(the_command_line == "updateShots"){
        updateShots();
        return true;
    }

    if(the_command_line == "updateScenes"){
        updateScenes();
        return true;
    }

    if(the_command_line == "updateCharacters"){
        updateCharacters();
        return true;
    }

    // Initialize LlamaClient if not already done (adjust model path as needed)
    if (!initializeLlamaClient()) {
        return false;
    }

    if(llamaModel){

        if(the_command_line == "lmstat"){
            QString info = llamaModel->getContextInfo();
            Log().info() << info.toUtf8().constData() << "\n";
            return true;
        }

        QString prompt = the_command_line;
        QString response = llamaModel->generateResponse(
            prompt,
            [](const QString &msg){
                Log().info() << msg.toUtf8().constData();
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            },
            [](const QString final){
                Log().info() << "\n";
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            }
            );
        qDebug() << "Response: " << response;
    }
    else
        return false;

    return true;
}

bool loadJsonWithDetails(const QString &filePath, QJsonDocument &docOut, QString &errorOut)
{
    QFile file(filePath);
    if (!file.exists()) {
        errorOut = QString("File does not exist: %1").arg(filePath);
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        errorOut = QString("Could not open file for reading: %1").arg(filePath);
        return false;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonParseError parseError;
    docOut = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        int offset = parseError.offset;
        int line = 1;
        int lineStart = 0;
        int lineEnd = jsonData.size();

        // Find line number
        for (int i = 0; i < offset && i < jsonData.size(); ++i) {
            if (jsonData[i] == '\n') {
                ++line;
                lineStart = i + 1;
            }
        }

        // Find end of line
        for (int i = offset; i < jsonData.size(); ++i) {
            if (jsonData[i] == '\n') {
                lineEnd = i;
                break;
            }
        }

        QByteArray lineContent = jsonData.mid(lineStart, lineEnd - lineStart).trimmed();

        errorOut = QString(
                       "JSON parse error in %1:\n"
                       "→ %2\n"
                       "At offset %3 (line %4):\n"
                       "%5")
                       .arg(filePath)
                       .arg(parseError.errorString())
                       .arg(offset)
                       .arg(line)
                       .arg(QString::fromUtf8(lineContent));
        return false;
    }

    return true;
}

void MainWindow::loadProject() {
    QString projectDir = QFileDialog::getExistingDirectory(this, "Select Project Folder");
    if (projectDir.isEmpty())
        return;

    if(scriptBreakdown)
        delete scriptBreakdown;
    scriptBreakdown = new ScriptBreakdown("");

    QString projectFilePath = QDir(projectDir).filePath("project.json");
    QJsonDocument projectDoc;
    QString errorMsg;
    if (!loadJsonWithDetails(projectFilePath, projectDoc, errorMsg)) {
        QMessageBox::critical(this, "Error", errorMsg);
        Log().info() << errorMsg.toUtf8().constData();
        return;
    }

    if (!projectDoc.isObject()) {
        QString msg = "project.json does not contain a top-level JSON object.";
        QMessageBox::critical(this, "Error", msg);
        Log().info() << msg.toUtf8().constData() << "\n";
        return;
    }

    // Validate required fields
    QJsonObject projectObj = projectDoc.object();
    QStringList requiredFields = { "name", "projectId", "fps" };
    QStringList missingFields;

    for (const QString &field : requiredFields) {
        if (!projectObj.contains(field)) {
            missingFields << field;
        }
    }

    if (!missingFields.isEmpty()) {
        QString msg = QString("Missing required field(s) in %1:\n→ %2")
                          .arg(projectFilePath)
                          .arg(missingFields.join(", "));
        QMessageBox::critical(this, "Invalid Project", msg);
        Log().info() << msg.toUtf8().constData();
        return;
    }

    // (Optional) Store project metadata
    this->currentProjectName = projectObj["projectName"].toString();
    this->currentProjectPath = projectDir;

    // Load scenes
    QDir scenesDir(QDir(projectDir).filePath("scenes"));

    qDebug() << "Scenes directory absolute path:" << scenesDir.absolutePath();
    Log().info() << "Looking for scenes in: " << scenesDir.absolutePath().toUtf8().constData();

    if (!scenesDir.exists()) {
        QMessageBox::critical(this, "Missing Folder", "scenes/ folder not found in project.");
        return;
    }

    qDebug() << "Scenes directory contains:" << scenesDir.entryList(QDir::AllEntries);
    Log().info() << "Entries in scenes directory:"
                 << scenesDir.entryList(QDir::AllEntries).join(", ").toUtf8().constData();


    bool foundErrors = false;
    QStringList errors;
    QStringList sceneFiles = scenesDir.entryList(QDir::Files);
    for (const QString &fileName : sceneFiles) {

        if (!fileName.endsWith(".json", Qt::CaseInsensitive)){
            Log().info() << "Skipping file "<< fileName.toUtf8().constData() << "\n";
            continue;
        }

        Log().info() << "Processing file "<< fileName.toUtf8().constData() << "\n";

        QString baseName = QFileInfo(fileName).completeBaseName(); // Strip .json
        QStringList parts = baseName.split('_');

        if (parts.size() < 2) {
            qDebug() << "Skipping malformed scene file:" << fileName;
            Log().info() << "Skipping malformed scene file:" << fileName.toUtf8().constData() << "\n";
            foundErrors = true;
            errors << "Skipping malformed scene file: " + fileName;
            continue;
        }

        QString sceneId = parts[0];  // "0001"
        QString sceneName = parts.mid(1).join('_'); // Rejoin the rest, e.g. "SCENE_002"


        QJsonDocument sceneDoc;
        QString errorMsg;
        QString sceneFilePath = scenesDir.filePath(fileName);

        if (!loadJsonWithDetails(sceneFilePath, sceneDoc, errorMsg)) {
            Log().info() << errorMsg.toUtf8().constData();
            foundErrors = true;
            errors << errorMsg;
            continue;
        }

        QJsonObject sceneObj;
        if(sceneDoc.isArray()) {
            sceneObj["shots"] = sceneDoc.array();
            sceneObj["sceneId"] = sceneId;
            sceneObj["name"] = sceneName;
        }
        else if (sceneDoc.isObject()) {
            sceneObj = sceneDoc.object();
            if(!sceneObj.contains("shots"))
                sceneObj["shots"] = QJsonArray();
            if(!sceneObj.contains("sceneId"))
                sceneObj["sceneId"] = sceneId;
            if(!sceneObj.contains("name"))
                sceneObj["name"] = sceneName;
        }
        else{
            qWarning() << "Invalid JSON format in" << fileName;
            Log().info() << "Invalid JSON format in" << fileName.toUtf8().constData() << "\n";
            errors << "Invalid JSON format in " + fileName;
            foundErrors = true;
            continue;
        }

        scriptBreakdown->loadScene(sceneName, sceneObj);  // Assuming you have this method
    }

    if(foundErrors){


        ErrorDialog errorDialog("Errors occurred while loading project", errors, this);
        errorDialog.exec();
    }
    else
        QMessageBox::information(this, "Project Loaded", "Project loaded successfully.");


    updateScenes();
    updateTimeline();
}

GameFusion::Panel* MainWindow::findPanelByUuid(const std::string& uuid) {

    auto& scenes = scriptBreakdown->getScenes();
    for (auto& scene : scenes) {
        for (auto& shot : scene.shots) {
            for(auto &panel : shot.panels){
                if (panel.uuid == uuid)
                    return &panel;
            }
        }
    }

    return nullptr;
}

void MainWindow::onTreeItemClicked(QTreeWidgetItem* item, int column) {
    QVariant uuidData = item->data(0, Qt::UserRole);
    if (!uuidData.isValid())
        return;

    QString uuid = uuidData.toString();

    // Use UUID to look up the panel in your project data
    const Panel* panel = findPanelByUuid(uuid.toStdString());
    if (!panel){
        Log().info() << "Panel " << uuid.toUtf8().constData() << " not found\n";
        return;
    }

    // TODO print panel selected info ...with Log().info() <<
    Log().info() << "Panel selected:\n"
                 << "  Name: " << panel->name.c_str() << "\n"
                 << "  Description: " << panel->description.c_str() << "\n"
                 << "  Start Frame: " << panel->startFrame << "\n"
                 << "  Duration: " << panel->durationFrames << "\n"
                 << "  Image: " << panel->image.c_str() << "\n"
                 << "  UUID: " << panel->uuid.c_str() << "\n";

    // Load and display the image, e.g. in a QLabel
    //QPixmap pixmap(QString::fromStdString(panel->image));
    //imageLabel->setPixmap(pixmap.scaled(imageLabel->size(), Qt::KeepAspectRatio));
    QString imagePath = currentProjectPath + "/movies/" + panel->image.c_str();
    paint->openImage(imagePath);
}
