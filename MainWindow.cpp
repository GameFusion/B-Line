


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
#include <QButtonGroup>

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

#include "GameCore.h" // for GameContext->gameTime()
#include "SoundServer.h"
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
#include "../TimeLineProject/TimelineOptionsDialog.h"

// END TimeLine related includes

using namespace GameFusion;


//#include "TimeLineWidget.h"


class FontAwesomeViewer : public QWidget
{
public:
    FontAwesomeViewer(QWidget *parent = nullptr) : QWidget(parent)
    {
        // Load Font Awesome font
        int fontId = QFontDatabase::addApplicationFont(":/fa-solid-900.ttf");
        //int fontId = QFontDatabase::addApplicationFont(":/fa-brands-400.ttf");
        if (fontId == -1) {
            qWarning() << "Failed to load Font Awesome font.";
            return;
        }

        QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
        if (fontFamilies.isEmpty()) {
            qWarning() << "No font families found.";
            return;
        }

        QFont fontAwesome(fontFamilies.at(0));
        fontAwesome.setPointSize(24);

        QWidget *containerWidget = new QWidget(this);  // Container widget for the scroll area
        QVBoxLayout *layout = new QVBoxLayout(containerWidget);

        // Iterate through a range of Unicode values
        for (int codePoint = 0xf000; codePoint <= 0xf3ff; ++codePoint) {
            QString iconText = QString(QChar(codePoint));
            QLabel *iconLabel = new QLabel(this);
            iconLabel->setFont(fontAwesome);
            iconLabel->setText(iconText + " " + QString::number(codePoint, 16).toUpper());
            layout->addWidget(iconLabel);
        }

        containerWidget->setLayout(layout);

        QScrollArea *scrollArea = new QScrollArea(this);
        scrollArea->setWidget(containerWidget);
        scrollArea->setWidgetResizable(true);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->addWidget(scrollArea);
        setLayout(mainLayout);

        setWindowTitle("Font Awesome Icons Viewer");
        resize(400, 600);
    }
};


TimeLineView* createTimeLine(QWidget &parent, MainWindow *myMainWindow)
{

    FontAwesomeViewer *viewer = new FontAwesomeViewer;
    viewer->show();

    parent.setWindowTitle("Timeline Viewer");
    parent.setStyleSheet("background-color: black;");

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
    QToolButton *addPanelButton = new QToolButton(&parent);
    addPanelButton->setText(QChar(0xf0fe));  // Shift left icon
    addPanelButton->setFont(fontAwesome);

    QToolButton *deletePanelButton = new QToolButton(&parent);
    deletePanelButton->setText(QChar(0xf146));  // Shift left icon
    deletePanelButton->setFont(fontAwesome);
/*
    QToolButton *addSegmentButton = new QToolButton(&parent);
    addSegmentButton->setText(QChar(0xf126));  // Shift left icon
    addSegmentButton->setFont(fontAwesome);

    QToolButton *deleteSegmentButton = new QToolButton(&parent);
    deleteSegmentButton->setText(QChar(0xf1f8));  // Shift left icon
    deleteSegmentButton->setFont(fontAwesome);
*/

    QToolButton *shiftLeftButton = new QToolButton(&parent);
    shiftLeftButton->setText(QChar(0xf2f6));  // Shift left icon
    shiftLeftButton->setFont(fontAwesome);

    QToolButton *shiftRightButton = new QToolButton(&parent);
    shiftRightButton->setText(QChar(0xf337));  // Group selection icon
    shiftRightButton->setFont(fontAwesome);
/*
    QToolButton *phonemeButton = new QToolButton(&parent);
    phonemeButton->setText(QChar(0xf1dd));  // Single selection icon
    phonemeButton->setFont(fontAwesome);
*/
    QToolButton *editButton = new QToolButton(&parent);
    editButton->setText(QChar(0xf044));  // Area selection icon
    editButton->setFont(fontAwesome);

    QToolButton *trashButton = new QToolButton(&parent);
    trashButton->setText(QChar(0xf1f8));  // Area selection icon
    trashButton->setFont(fontAwesome);
/*
    QToolButton *anotateButton = new QToolButton(&parent);
    anotateButton->setText(QChar(0xf3c5));  // Area selection icon
    // or pin with F3C5
    anotateButton->setFont(fontAwesome);
    */

    QToolButton *backwardFastButton = new QToolButton(&parent);
    backwardFastButton->setText(QChar(0xf049));  // Area selection icon
    backwardFastButton->setFont(fontAwesome);
    // todo add connection to go to prior scene

    QToolButton *backwardStepButton = new QToolButton(&parent);
    backwardStepButton->setText(QChar(0xf048));  // Area selection icon
    backwardStepButton->setFont(fontAwesome);
    // todo add connection to go to prior shot

    QToolButton *playButton = new QToolButton(&parent);
    playButton->setText(QChar(0xf04b));  // Area selection icon
    playButton->setFont(fontAwesome);
    // todo add connection to play

    QToolButton *pauseButton = new QToolButton(&parent);
    pauseButton->setText(QChar(0xf04c));  // Area selection icon
    pauseButton->setFont(fontAwesome);
    // todo add connection to go pause

    QToolButton *forwardStepButton = new QToolButton(&parent);
    forwardStepButton->setText(QChar(0xf051));  // Area selection icon
    forwardStepButton->setFont(fontAwesome);
    // todo add connection to go to next shot

    QToolButton *forwardFastButton = new QToolButton(&parent);
    forwardFastButton->setText(QChar(0xf050));  // Area selection icon
    forwardFastButton->setFont(fontAwesome);
    // todo add connection to go to next scene

    QToolButton *settingsButton = new QToolButton(&parent);
    settingsButton->setText(QChar(0xf1de));  // Single selection icon
    settingsButton->setFont(fontAwesome);
    ///

    // Create and configure the TimeLineView
    //CustomGraphicsScene *scene = new CustomGraphicsScene();
    TimeLineView *timelineView = new TimeLineView(&parent);

    //timelineView->setScene(scene);
    QGraphicsScene *scene = timelineView->scene();
    timelineView->setMinimumSize(80, 60); // Set a minimum size for the timeline view
    //timelineView->scale(0.5, 1);

    // Create a QSpinBox for scale control
    QSpinBox *scaleControl = new QSpinBox(&parent);
    scaleControl->setFixedWidth(100);
    scaleControl->setRange(10, 200); // Set range from 1x to 10x
    scaleControl->setValue(100); // Initial scale factor set to 1
    scaleControl->setPrefix("Scale: ");
    //scaleControl->setSuffix("x");

    // Create a QSlider for scale control
    //QSlider *scaleSlider = new QSlider(Qt::Horizontal, &parent);
    CustomSlider *scaleSlider = new CustomSlider(Qt::Horizontal, &parent);
    scaleSlider->setRange(10, 200); // Matching range with the spin box
    scaleSlider->setValue(100); // Initial value matching the spin box
    scaleSlider->setFixedWidth(400);

    // Create a new QGraphicsView as a custom scrollbar
    ScrollbarView *scrollbarView = new ScrollbarView(&parent);
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
    Track *track1 = new Track("Track 1", 0, 500000, TrackType::Storyboard); // Name, Start Time, Duration in seconds
    Track *track2 = new Track("Track 2", 0, 150000, TrackType::NonLinearMedia); // Name, Start Time, Duration in seconds
    Track *track3 = new Track("Track 3", 0, 170000, TrackType::NonLinearMedia); // Name, Start Time, Duration in seconds

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
    QComboBox *styleComboBox = new QComboBox(&parent);
    styleComboBox->addItem("Grayscale");
    styleComboBox->addItem("Inferno");
    styleComboBox->addItem("Viridis");
    styleComboBox->addItem("Cividis");
    styleComboBox->addItem("Plasma");
    styleComboBox->addItem("Turbo");
    styleComboBox->addItem("Roseus");

    styleComboBox->addItem("None");

    // Create a checkbox for waveform display
    QCheckBox *waveformCheckbox = new QCheckBox("Waveform", &parent);
    waveformCheckbox->setChecked(false);

    QSpacerItem *spacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    QVBoxLayout *toplayout = new QVBoxLayout(&parent);
    QHBoxLayout *hlayout = new QHBoxLayout(&parent);

    QVBoxLayout *gfxlayout = new QVBoxLayout(&parent);

    gfxlayout->setSpacing(0);  // Remove spacing between the elements

    // Grouping for toggle buttons (play/pause)
    QButtonGroup *toggleGroup = new QButtonGroup(&parent);
    toggleGroup->setExclusive(true);
    toggleGroup->addButton(playButton);
    toggleGroup->addButton(pauseButton);

    // Setup for play/pause toggle buttons without borders
    // Play Button
    playButton->setCheckable(true);
    playButton->setStyleSheet(
        "QToolButton { "
        "    border: 2px solid black; "  // Black border when not selected
        "    color: #32CD32; "           // LimeGreen text color
        "    background: black; "  // Transparent background
        "    border-radius: 5px; "       // Rounded corners with 5-pixel radius
        "} "
        "QToolButton:checked { "
        "    background-color: #32CD32; "  // LimeGreen background when selected
        "    border: 2px solid #32CD32; "  // LimeGreen border when selected
        "    color: white; "               // Optionally change the text color to white when selected
        "    border-radius: 5px; "
        "} "
        );

    // Pause Button
    pauseButton->setCheckable(true);
    pauseButton->setStyleSheet(
        "QToolButton { "
        "    border: 2px solid black; "  // Black border when not selected
        "    color: #FF4500; "           // OrangeRed text color
        "    background: black; "  // Transparent background
        "    border-radius: 5px; "       // Rounded corners with 5-pixel radius
        "} "
        "QToolButton:checked { "
        "    background-color: #FF4500; "  // OrangeRed background when selected
        "    border: 2px solid #FF4500; "  // OrangeRed border when selected
        "    color: white; "               // Optionally change the text color to white when selected
        "    border-radius: 5px; "
        "} "
        );

    hlayout->addWidget(scaleControl); // Add the scale control at the top
    hlayout->addWidget(scaleSlider); // Add the scale control at the top
    hlayout->addWidget(styleComboBox);  // Add the style combobox
    hlayout->addWidget(waveformCheckbox);

    hlayout->addWidget(addPanelButton);
    hlayout->addWidget(deletePanelButton);
    //hlayout->addWidget(addSegmentButton);
    //hlayout->addWidget(deleteSegmentButton);


    hlayout->addWidget(shiftLeftButton);
    hlayout->addWidget(shiftRightButton);
    //hlayout->addWidget(phonemeButton);
    hlayout->addWidget(editButton);
    hlayout->addWidget(trashButton);
    //hlayout->addWidget(anotateButton);
    hlayout->addWidget(backwardFastButton);
    hlayout->addWidget(backwardStepButton);
    hlayout->addWidget(playButton);
    hlayout->addWidget(pauseButton);
    hlayout->addWidget(forwardStepButton);
    hlayout->addWidget(forwardFastButton);
    hlayout->addWidget(settingsButton);
    //hlayout->addWidget(singleSelectionButton);
    //hlayout->addWidget(areaSelectionButton);

    hlayout->addSpacerItem(spacer);
    gfxlayout->addWidget(timelineView);
    gfxlayout->addWidget(scrollbarView);

    toplayout->addLayout(hlayout);
    toplayout->addLayout(gfxlayout);


    parent.setLayout(toplayout);

    QObject::connect(addPanelButton, &QToolButton::clicked, [timelineView]() {
        timelineView->onAddPanel();
    });

    QObject::connect(deletePanelButton, &QToolButton::clicked, [timelineView]() {
        timelineView->onDeletePanel();
    });

    QObject::connect(playButton, &QToolButton::clicked, myMainWindow, &MainWindow::play);
    QObject::connect(pauseButton, &QToolButton::clicked, myMainWindow, &MainWindow::pause);
    QObject::connect(forwardStepButton, &QToolButton::clicked, myMainWindow, &MainWindow::nextShot);
    QObject::connect(forwardFastButton, &QToolButton::clicked, myMainWindow, &MainWindow::nextScene);
    QObject::connect(backwardStepButton, &QToolButton::clicked, myMainWindow, &MainWindow::prevShot);
    QObject::connect(backwardFastButton, &QToolButton::clicked, myMainWindow, &MainWindow::prevScene);

    QObject::connect(settingsButton, &QToolButton::clicked, [timelineView]() {
        timelineView->onOptionsDialog();
    });

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
    parent.show();

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

    QAction *importAudioTrackAction = ui->menuFile->addAction(tr("&Import Audio Track"));
    connect(importAudioTrackAction, &QAction::triggered, this, &MainWindow::importAudioTrack);

	QTimer *timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(update()));
	timer->start(1000);

    playbackTimer = new QTimer(this);
    connect(playbackTimer, &QTimer::timeout, this, &MainWindow::onPlaybackTick);

	setAcceptDrops(true);

    QObject::connect(ui->actionOpen, SIGNAL(triggered()), this, SLOT(loadProject()));
    QObject::connect(ui->actionNew, SIGNAL(triggered()), this, SLOT(newProject()));
    QObject::connect(ui->actionSave, SIGNAL(triggered()), this, SLOT(saveProject()));
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

    shotPanel = new ShotPanelWidget;
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

    timeLineView = createTimeLine(*ui->timeline, this);

    connect(timeLineView, &TimeLineView::timeCursorMoved,
            this, &MainWindow::onTimeCursorMoved);
    connect(timeLineView, &TimeLineView::addPanel,
            this, &MainWindow::addPanel);
    connect(timeLineView, &TimeLineView::deletePanel,
            this, &MainWindow::deletePanel);
    connect(timeLineView, &TimeLineView::optionsDialog,
            this, &MainWindow::timelineOptions);

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

void addShot(QTreeWidgetItem* shotItem, const GameFusion::Shot &shot, float fps) {

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

    float mspf = fps > 0 ? 1000./fps : 1;

    if (!shot.panels.empty()) {
        for (const auto& panel : shot.panels) {
            QTreeWidgetItem* panelItem = new QTreeWidgetItem(shotItem);
            panelItem->setText(0, QString::fromStdString(panel.name));
            panelItem->setText(1, QString::fromStdString(panel.description));
            panelItem->setText(2, QString::number(panel.startTime/mspf));
            panelItem->setText(3, QString::number(panel.durationTime/mspf));
            panelItem->setText(4, QString::fromStdString(panel.uuid));

            // Store UUID or full Panel pointer (if lifetime is stable)
            panelItem->setData(0, Qt::UserRole, QString::fromStdString(panel.uuid));
        }
    }
}

void MainWindow::importAudioTrack(){
    QString fileName = QFileDialog::getOpenFileName(this, tr("Import Audio File Track"), "", tr("Audio Files (*.wav; *.aiff)"));;

    if (fileName.isEmpty()) {
        return; // User canceled
    }

    Track *track = new Track("Track 3", 0, 150000, TrackType::NonLinearMedia);

    TrackItem *trx = timeLineView->addTrack(track);
    trx->loadAudio("WarnerTest", fileName.toUtf8().constData());
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

    float fps = projectJson["fps"].toDouble();

    scriptBreakdown = new ScriptBreakdown(fileName.toStdString().c_str(), fps, dictionary, dictionaryCustom, llamaClient);


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
                        addShot(shotItem, shot, fps);

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
                    addShot(shotItem, shot, fps);
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

    float fps = projectJson["fps"].toDouble();

    const auto& shots = scriptBreakdown->getShots();
    for (const auto& shot : shots) {
        QTreeWidgetItem* shotItem = new QTreeWidgetItem(ui->shotsTreeWidget);
        addShot(shotItem, shot, fps);
    }

    ui->shotsTreeWidget->expandAll();
}

void MainWindow::updateScenes(){
    if(!scriptBreakdown) return;

    ui->shotsTreeWidget->clear();

    float fps = projectJson["fps"].toDouble();

    const auto& scenes = scriptBreakdown->getScenes();
    for (const auto& scene : scenes) {
        QTreeWidgetItem* sceneItem = new QTreeWidgetItem(ui->shotsTreeWidget);
        sceneItem->setText(0, QString::fromStdString(scene.name));
        sceneItem->setIcon(1, QIcon("scene_icon.png"));
        for (const auto& shot : scene.shots) {
            QTreeWidgetItem* shotItem = new QTreeWidgetItem(sceneItem);
            addShot(shotItem, shot, fps);
        }
    }

    ui->shotsTreeWidget->expandAll();
}

void MainWindow::updateTimeline(){

    auto& scenes = scriptBreakdown->getScenes();

    episodeDuration.durationMs = 0;
    episodeDuration.frameCount = 0;

    TrackItem * track = timeLineView->getTrack(0);
    QGraphicsScene *gfxscene = timeLineView->scene();

    //gfxscene->clear();

    qreal fps = 25;
    qreal mspf = 1000.f/fps;
    //long startTimeFrame = episodeDuration.frameCount;
    long startTime = 0;
    for (auto& scene : scenes) {

        Log().info() << "Scene Start Frame: " << scene.name.c_str() << " " << startTime << "\n";

        CursorItem *sceneMarker = timeLineView->addSceneMarker(startTime, scene.name.c_str());
        sceneMarker->setUuid(scene.uuid.c_str());

        for (auto& shot : scene.shots) {

            //long shotFrameStart = startTimeFrame;  // place shot at current frame
            qreal shotTimeStart = shot.startTime == -1 ? startTime : shot.startTime;
            qreal shotTimeEnd = shot.endTime == -1 ? shotTimeStart + shot.frameCount * mspf : shot.endTime;
            qreal shotDuration = shotTimeEnd - shotTimeStart;

            ShotSegment* segment = new ShotSegment(gfxscene, shotTimeStart, shotDuration);
            segment->setUuid(shot.uuid.c_str());

            if(sceneMarker) {
                // here we are at the
                segment->setSceneMarker(sceneMarker);
                //

                // TODO create a function to set (link) the sceneMarker (or uuid) to ShotSegment
                // we want this link for the start of a scene !

            /*
             * TODO, add sceneMarker uuid to ShotSegment !!!!
             *
             *
             *
             */
                sceneMarker = null;
            }

            segment->setSegmentUpdateCallback([this, mspf](Segment *segment) {
                ShotContext shotContext = findShotByUuid(segment->getUuid().toUtf8().constData());
                if (shotContext.isValid()) {
                    shotContext.scene->dirty = true;
                    Shot *shot = shotContext.shot;

                    //shot->startTime = newStartTime;
                    //shot->endTime = newStartTime + newDuration;


                    shot->startTime = segment->timePosition();
                    shot->endTime = segment->timePosition() + segment->getDuration();

                    shot->frameCount = segment->getDuration() / mspf;

                    Log().info() << "Updated shot "<< shot->name.c_str() <<"with UUID: " << shot->uuid.c_str()
                                 << ", new start time: " << shot->startTime
                                << ", new end time: " << shot->endTime <<
                        "\n";

                    ShotSegment *shotSegment = dynamic_cast<ShotSegment*>(segment);
                    if (shotSegment) {
                        CursorItem *sceneMarker = shotSegment->getSceneMarkerPointer();
                        if (sceneMarker)
                            sceneMarker->setTimePosition(segment->timePosition());
                    }

                } else {
                    Log().info() << "Shot not found for UUID: " << segment->getUuid().toUtf8().constData() << "\n";
                }
            });



            segment->setMarkerUpdateCallback([this](const QString& uuid, float newStartTime, float newDuration) {
                PanelContext panelContext = findPanelByUuid(uuid.toStdString());
                if (panelContext.isValid()) {

                    panelContext.scene->dirty = true;

                    Panel *panel = panelContext.panel;
                    panel->startTime = newStartTime;
                    panel->durationTime = newDuration;
                    Log().info() << "Updated panel with UUID: " << uuid.toUtf8().constData()
                                 << ", new start time: " << panel->startTime
                                 << ", new duration: " << panel->durationTime <<
                        "\n";
                } else {
                    Log().info() << "Panel not found for UUID: " << uuid.toUtf8().constData() << "\n";
                }
            });

            if(shot.startTime ==-1){
                shot.startTime = shotTimeStart;
                shot.endTime = shotTimeStart + shotDuration;

            }

            Log().info() << "  Shot Start Frame: " << shot.name.c_str() << " " << (int)(startTime/mspf)
                         << " ms start: " << (float)shotTimeStart << "\n";

            segment->marker()->setShotLabel(shot.name.c_str());
            segment->marker()->setPanelName(shot.name.c_str());

            int pannelIndex(0);

            for (const auto& panel : shot.panels) {
                qreal panelTimeStart = startTime + panel.startTime;
                //qreal panelTimeStart = panelFrameStart * mspf;
                //qreal panelDuration = panel.durationFrames * mspf;
                QString thumbnail = currentProjectPath + "/movies/" + panel.thumbnail.c_str();

                // by default ShotSegment had one initial panel
                PanelMarker *panelMarker = pannelIndex == 0 ? segment->marker() : new PanelMarker(0, 30, 0, 220, segment, panel.name.c_str(), "", "");

                panelMarker->setPanelName(QString::fromStdString(panel.name));
                panelMarker->loadThumbnail(thumbnail);
                panelMarker->setUuid(panel.uuid.c_str());

                panelMarker->setStartTimePos(panel.startTime);
                pannelIndex ++;
            }

            segment->updateMarkersEndTime();

            track->addSegment(segment);

            // Advance frame cursor by shot's frame count
            startTime = shot.endTime;
        }

        // After each scene, update the global episode duration
        episodeDuration.frameCount = startTime / mspf;
        episodeDuration.durationMs = startTime;
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

    loadProject(projectDir);
}

void MainWindow::loadProject(QString projectDir){
    if(scriptBreakdown)
        delete scriptBreakdown;

    timeLineView->clear();

    currentPanel = nullptr;

    paint->newImage();

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
    //QJsonObject projectObj = projectDoc.object();
    projectJson = projectDoc.object();
    QStringList requiredFields = { "projectName", "fps" };
    QStringList missingFields;

    for (const QString &field : requiredFields) {
        if (!projectJson.contains(field)) {
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
    this->currentProjectName = projectJson["projectName"].toString();
    this->currentProjectPath = projectDir;

    float fps = projectJson["fps"].toDouble();
    scriptBreakdown = new ScriptBreakdown("", fps);

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

        scriptBreakdown->loadScene(sceneName, sceneObj, QFileInfo(fileName).fileName());  // Assuming you have this method
    }

    if(foundErrors){


        ErrorDialog errorDialog("Errors occurred while loading project", errors, this);
        errorDialog.exec();
    }
    else {
        //QMessageBox::information(this, "Project Loaded", "Project loaded successfully.");
        Log().info() << "Project loaded successfully.\n";
    }

    updateScenes();
    updateTimeline();
}

ShotContext MainWindow::findShotByUuid(const std::string& uuid) {

    auto& scenes = scriptBreakdown->getScenes();
    for (auto& scene : scenes) {
        for (auto& shot : scene.shots) {
            if (shot.uuid == uuid)
                    return { &scene, &shot };
        }
    }

    return {};
}

PanelContext MainWindow::findPanelByUuid(const std::string& uuid) {

    auto& scenes = scriptBreakdown->getScenes();
    for (auto& scene : scenes) {
        for (auto& shot : scene.shots) {
            for(auto &panel : shot.panels){
                if (panel.uuid == uuid)
                    return { &scene, &shot, &panel };
            }
        }
    }

    return {};
}

PanelContext MainWindow::findPanelForTime(double time, double threshold) {

    if(!scriptBreakdown){
        Log().info() << "Script Breakdown is empty\n";
        return {};
    }
    auto& scenes = scriptBreakdown->getScenes();
    for (auto& scene : scenes) {
        for (auto& shot : scene.shots) {
            if(time < shot.startTime || time > shot.endTime)
                continue;
            for(auto &panel : shot.panels){
                if (time >= (shot.startTime + panel.startTime) &&
                    time < (shot).startTime + (panel.startTime + panel.durationTime)) {

                    return { &scene, &shot, &panel };;
                }
            }
        }
    }

    return {};
}

ShotContext MainWindow::findShotForTime(double time/*, double buffer*/) {
    if (!scriptBreakdown) {
        Log().info() << "Script Breakdown is null\n";
        return {};
    }

    auto& scenes = scriptBreakdown->getScenes();
    for (auto& scene : scenes) {
        for (size_t i = 0; i < scene.shots.size(); ++i) {
            auto& shot = scene.shots[i];

            // 1. Direct match inside this shot
            if (time >= shot.startTime && time < shot.endTime) {
                return { &scene, &shot };
            }

            // 2. Between this shot and the next
            if (i + 1 < scene.shots.size()) {
                auto& nextShot = scene.shots[i + 1];
                if (time >= shot.endTime && time < nextShot.startTime) {
                    // Optional: insert logic could go here
                    Log().info() << "Time is between Shot " << shot.name.c_str()
                                 << " and Shot " << nextShot.name.c_str() << "\n";
                    return { &scene, nullptr }; // indicates an insert opportunity
                }
            }
        }

        // 3. After the last shot in the scene
        if (!scene.shots.empty() && time >= scene.shots.back().endTime) {
            Log().info() << "Time is after last shot in Scene " << scene.name.c_str() << "\n";
            return { &scene, nullptr }; // new shot can be appended
        }
    }

    Log().info() << "No suitable scene found for time " << (float)time << "\n";
    return {};
}

void MainWindow::onTreeItemClicked(QTreeWidgetItem* item, int column) {
    QVariant uuidData = item->data(0, Qt::UserRole);
    if (!uuidData.isValid())
        return;

    QString uuid = uuidData.toString();

    // Use UUID to look up the panel in your project data
    PanelContext panelContext = findPanelByUuid(uuid.toStdString());



    Scene *scene = panelContext.scene;
    Shot *shot = panelContext.shot;
    Panel *panel = panelContext.panel;

    if (!panel){
        Log().info() << "Panel " << uuid.toUtf8().constData() << " not found\n";
        return;
    }

    timeLineView->getTrack(0)->clearCurrentPanelMarkers();
    Segment* seg = timeLineView->getTrack(0)->getSegmentByUuid(shot->uuid.c_str());
    if(seg){
        PanelMarker *panelMarker = (PanelMarker*)seg->getMarkerItemByUuid(panel->uuid.c_str());
        if(panelMarker) {
            panelMarker->setIsCurrent(!panelMarker->getIsCurrent());
        }
    }

    // TODO print panel selected info ...with Log().info() <<
    Log().info() << "Panel selected:\n"
                 << "  Name: " << panel->name.c_str() << "\n"
                 << "  Description: " << panel->description.c_str() << "\n"
                 << "  Start Time: " << panel->startTime << "\n"
                 << "  Duration: " << panel->durationTime << "\n"
                 << "  Image: " << panel->image.c_str() << "\n"
                 << "  UUID: " << panel->uuid.c_str() << "\n";

    // Load and display the image, e.g. in a QLabel
    //QString imagePath = currentProjectPath + "/movies/" + panel->image.c_str();
    //paint->openImage(imagePath);

    // set time cursor to panel start time
    timeLineView->setTimeCursor(shot->startTime + panel->startTime);

    // Update the panel UI
    float fps = projectJson["fps"].toDouble();
    shotPanel->setPanelInfo(scene, shot, panel, fps);
}

#include "NewProjectDialog.h"


void MainWindow::newProject()
{
    NewProjectDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QString name = dialog.projectName();
    QString location = dialog.location();
    QString fullPath = location + "/" + name;

    // Create project folder
    QDir dir;
    if (!dir.mkpath(fullPath)) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to create project folder."));
        return;
    }

    // Define project directory structure
    const QStringList subDirs = {
        "assets",
        "assets/LoRA",
        "assets/audio",
        "assets/controlnet",
        "movies",
        "panels",
        "references",
        "references/characters",
        "references/environments",
        "references/props",
        "scenes",
        "scripts"
    };

    // Create all subdirectories
    for (const QString& subDir : subDirs) {
        if (!dir.mkpath(fullPath + "/" + subDir)) {
            QMessageBox::warning(this, tr("Error"),
                                 tr("Failed to create folder: %1").arg(subDir));
            return;
        }
    }

    // Parse resolution as array
    QString resString = dialog.resolution(); // e.g. "1920x1080"
    QStringList resParts = resString.split('x');
    int resWidth = resParts.value(0).toInt();
    int resHeight = resParts.value(1).toInt();

    // Generate project UUID
    QString projectUUID = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // Generate ISO timestamp
    QString createdTime = QDateTime::currentDateTimeUtc().toString(Qt::ISODate) + "Z";

    // Write project metadata
    //QJsonObject projectJson;

    projectJson = QJsonObject();

    projectJson["aspectRatio"] = dialog.aspectRatio();
    projectJson["director"] = dialog.director();
    projectJson["estimatedDuration"] = dialog.estimatedDuration();
    projectJson["fps"] = dialog.fps();
    projectJson["notes"] = dialog.notes();
    projectJson["projectCode"] = dialog.projectCode();
    projectJson["projectName"] = name;
    projectJson["resolution"] = QJsonArray{ resWidth, resHeight };
    projectJson["safeFrame"] = dialog.safeFrame();
    projectJson["created"] = createdTime;
    projectJson["version"] = 1;
    projectJson["saveFormat"] = "compact";
    projectJson["scenesPath"] = "scenes";
    projectJson["projectId"] = name;

    QFile metadataFile(fullPath + "/project.json");
    if (!metadataFile.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to save project metadata."));
        return;
    }

    QJsonDocument doc(projectJson);
    metadataFile.write(doc.toJson());
    metadataFile.close();

    QMessageBox::information(this, tr("Success"), tr("New project created successfully."));

    // Optional: load project into workspace
    loadProject(fullPath);
}

void MainWindow::onTimeCursorMoved(double time)
{
    // Respond to timeline cursor change
    //qDebug() << "Timeline cursor moved to time:" << time;
    Log().debug() << "Timeline cursor moved to time:" << (float)time << "\n";
    // Update other UI parts if needed


    // Find the current Scene → Shot → Panel at given time
    /*
     * Find the current Scene → Shot → Panel at given time
     *
     *
     *
     * Project
     * └─ Scenes (each has Shots)
     *      └─ Shots (each has Panels)
     *           └─ Panels (each has startTime, endTime, imagePath or image)
     */

    Panel* newPanel = nullptr;

    long timeCounter = 0;

    float fps = projectJson["fps"].toDouble();
    if(fps <= 0)
    {
        Log().info() << "Invalid project fps " << fps << "\n";
        return;
    }
    float mspf = 1000 / fps;

    auto& scenes = scriptBreakdown->getScenes();

    for (auto& scene : scenes) {
        for (auto& shot : scene.shots) {

            if(shot.startTime == -1) {
                shot.startTime = timeCounter;
                shot.endTime = shot.startTime + shot.frameCount*mspf;
            }

            timeCounter = shot.endTime;

            if(time < shot.startTime || time > shot.endTime)
                continue;

            for(auto &panel : shot.panels){
                if (time >= (shot.startTime + panel.startTime) &&
                    time < (shot).startTime + (panel.startTime + panel.durationTime)) {
                    newPanel = &panel;

                    break;
                }
            }

            if(newPanel)
                break;
        }
        if(newPanel)
            break;
    }



    // If no panel found, show white image
    if (!newPanel) {
        Log().debug() << "No panel at time: " << (float)time << ", showing blank image.";
        paint->newImage();
        currentPanel = nullptr;
        return;
    }

    if(newPanel == currentPanel)
        return; // no panel change

    currentPanel = newPanel;

    QString imagePath = currentProjectPath + "/movies/" + currentPanel->image.c_str();
    // Display the panel image, or a white image if not available
    if (!currentPanel->image.empty() && QFile::exists(imagePath)) {
        paint->openImage(imagePath);
        Log().debug() << "Loaded image: " << imagePath.toUtf8().constData() << "\n";
    } else {
        Log().debug() << "Panel image not found, showing white frame.";

        paint->newImage();
    }
}

void MainWindow::saveProject(){

    if(scriptBreakdown){
        scriptBreakdown->saveModifiedScenes(currentProjectPath);
    }

}

QString generateUniquePanelName(GameFusion::Shot* shot) {
    if (!shot)
        return "Panel1";

    int index = 1;
    QString baseName = "Panel";
    QString candidateName;
    bool nameExists = false;

    do {
        candidateName = baseName + QString::number(index++);
        nameExists = std::any_of(shot->panels.begin(), shot->panels.end(), [&](const Panel& panel) {
            return QString::fromStdString(panel.name) == candidateName;
        });
    } while (nameExists);

    return candidateName;
}

void MainWindow::addPanel(double t) {
    Log().info() << "Add Panel @ "<<(float)t<<"\n";



    PanelContext panelContext = findPanelForTime(t);
    if(panelContext.isValid()){
        Log().info() << "Found panel in shot: Scene " << panelContext.scene->name.c_str()
                     << ", Shot " << panelContext.shot->name.c_str()
                     << ", uuid " << panelContext.panel->name.c_str() << ", uuid << " <<panelContext.panel->uuid.c_str() <<"\n";

        qreal panelTimeStart = t - panelContext.shot->startTime;

        bool shiftPressed = QApplication::keyboardModifiers() & Qt::ShiftModifier;

        Panel newPanel;
        if (shiftPressed) {
            Log().info() << "Shift pressed: creating brand new panel (not a duplicate)";
            newPanel = Panel(); // default constructor, empty panel
            newPanel.startTime = panelTimeStart;
            //newPanel.durationTime = 1.0; // or any default duration
            newPanel.name = generateUniquePanelName(panelContext.shot).toUtf8().constData(); // you need to define this
            newPanel.thumbnail = ""; // or generate default thumbnail
            //newPanel.uuid = generateUuid(); // ensure uniqueness
        } else {
            newPanel = panelContext.panel->duplicatePanel(panelTimeStart);
        }

        // Optional: Insert a new panel *after* the found one
        //Panel newPanel = panelContext.panel->duplicatePanel(panelTimeStart); // If you're duplicating

        //insertPanelAfter(panelContext, newPanel);


        // add panel to time line
        // get ShotSegment for panelContext.shot.uuid
        TrackItem * track = timeLineView->getTrack(0);
        if(!track)
            return;

        Segment *segment = track->getSegmentByUuid(panelContext.shot->uuid.c_str());
        if(segment){

            QString thumbnail = currentProjectPath + "/movies/" + newPanel.thumbnail.c_str();

            PanelMarker *panelMarker = new PanelMarker(0, 30, 0, 220, segment, newPanel.name.c_str(), "", "");

            panelMarker->setPanelName(QString::fromStdString(newPanel.name));
            if(!newPanel.thumbnail.empty())
                panelMarker->loadThumbnail(thumbnail);
            panelMarker->setUuid(newPanel.uuid.c_str());

            panelMarker->setStartTimePos(panelTimeStart);

            //segment->updateMarkersEndTime();
            int h = segment->getHeight();
            panelMarker->updateHeight(h);

            //newPanel.durationTime = panelMarker->duration();
            panelContext.shot->panels.push_back(newPanel);

            syncPanelDurations(segment, panelContext.shot);
        }

    }
    else {
        Log().info() << "No panel found at time " << (float)t << ". Creating new panel...\n";

        /*
        ShotContext shotContext = findShotForTime(t);

        if (!shotContext.shot) {


            // Insert a new shot here
            QString newShotName = generateUniqueShotName(shotContext.scene);
            Shot newShot;
            newShot.name = newShotName.toStdString();
            newShot.startTime = t;
            newShot.endTime = t + 1.0; // 1 second duration

            Panel defaultPanel;
            defaultPanel.startTime = 0.0;
            defaultPanel.durationTime = 1.0;

            newShot.panels.push_back(defaultPanel);
            shotContext.scene->shots.push_back(newShot); // Or insert into correct position

            Log().info() << "Created new shot " << newShot.name.c_str() << " with one default panel\n";
        }
        else
        {
            Log().info() << "Create new Panel in empty Shot\n";
        }
*/
    }
}

void MainWindow::deletePanel(double t) {
    Log().info() << "Delete Panel @ "<<(float)t<<"\n";

    PanelContext panelContext = findPanelForTime(t);

    if(panelContext.isValid()){
        TrackItem * track = timeLineView->getTrack(0);
        Segment *segment = track->getSegmentByUuid(panelContext.shot->uuid.c_str());
        if(segment){
            bool removedMarker = segment->removeMarkerItemByUuid(panelContext.panel->uuid.c_str());
            if (removedMarker) {
                Log().info() << "Marker deleted on timeline.\n";

                bool removedPanel = panelContext.shot->removePanelByUuid(panelContext.panel->uuid);
                if(removedPanel)
                    Log().info() << "Panel deleted.\n";
            }

            syncPanelDurations(segment, panelContext.shot);
        }
    }
}

QString MainWindow::generateUniqueShotName(Scene* scene) {
    int index = 1;
    QString base = "SHOT";
    while (true) {
        QString candidate = QString("%1_%2").arg(base).arg(index, 3, 10, QChar('0'));
        auto it = std::find_if(scene->shots.begin(), scene->shots.end(),
                               [&](const Shot& s) { return s.name.c_str() == candidate; });
        if (it == scene->shots.end())
            return candidate;
        ++index;
    }
}

void MainWindow::syncPanelDurations(Segment* segment, Shot* shot)
{
    if (!segment || !shot)
        return;

    segment->updateMarkersEndTime();

    for (auto& panel : shot->panels) {
        MarkerItem* panelMarker = segment->getMarkerItemByUuid(panel.uuid.c_str());
        if (panelMarker)
            panel.durationTime = panelMarker->duration();
    }
}

void MainWindow::timelineOptions(){
    TimelineOptionsDialog optionsDialog(this);

    optionsDialog.setOptionsFromTimeline(timeLineView); // <- prefill dialog with timeline state

    if (optionsDialog.exec() == QDialog::Accepted) {
        QString unitsHeader = optionsDialog.getDisplayUnitsHeader();
        QString unitsCursor = optionsDialog.getDisplayUnitsCursor();
        bool snapToFrame = optionsDialog.getSnapToFrame();
        int startTime = optionsDialog.getStartTime();
        int endTime = optionsDialog.getEndTime();
        double playbackSpeed = optionsDialog.getPlaybackSpeed();
        double frameRate = optionsDialog.getFrameRate();

        // Handle the options
        qDebug() << "Header Units:" << unitsHeader;
        qDebug() << "Cursor Units:" << unitsCursor;
        qDebug() << "Snap to Frame:" << snapToFrame;
        qDebug() << "Start Time:" << startTime;
        qDebug() << "End Time:" << endTime;
        qDebug() << "Playback Speed:" << playbackSpeed;
        qDebug() << "Frame Rate:" << frameRate;

        // Apply these options to the timeline
        optionsDialog.applyToTimeline(timeLineView);     // <- apply changes
    }
}

void MainWindow::play() {
    qDebug() << "Play pressed";

    if (isPlaying) return;

    isPlaying = true;

    // If range not set, default to full timeline length
    if (playbackEnd <= playbackStart)
        playbackEnd = 10000 * 1000; // e.g., 10 milliseconds

    currentPlayTime = playbackStart;
    timeLineView->setTimeCursor(currentPlayTime);

    GameFusion::SoundServer::context()->seek(currentPlayTime);
    GameFusion::SoundServer::context()->play();

    playbackTimer->start(playbackIntervalMs);
}

void MainWindow::pause() {
    qDebug() << "Pause pressed";

    GameFusion::SoundServer::context()->stop();

    if (!isPlaying) return;


    playbackTimer->stop();
    isPlaying = false;
}

void MainWindow::stop() {

    GameFusion::SoundServer::context()->stop();

    if (!isPlaying) return;


    playbackTimer->stop();
    isPlaying = false;
    currentPlayTime = playbackStart;
    timeLineView->setTimeCursor(currentPlayTime);
}

void MainWindow::nextShot() {
    qDebug() << "Next Shot pressed";
    // TODO: Implement forward functionality
    timeLineView->gotoNextSegment();
}

void MainWindow::prevShot() {
    qDebug() << "Prev Shot pressed";
    // TODO: Implement backward functionality
    timeLineView->gotoPrevSegment();
}

void MainWindow::nextScene() {
    qDebug() << "Next Scene pressed";
    // TODO: Implement forward functionality
    timeLineView->gotoNextScene();
}

void MainWindow::prevScene() {
    qDebug() << "Prev Scene pressed";
    // TODO: Implement backward functionality
    timeLineView->gotoPrevScene();
}

void MainWindow::onPlaybackTick(){

    // TODO implement a system to collect the


    //currentPlayTime += playbackIntervalMs;
    //long currentPlayTime = *GameFusion::GameContext->gameTime();
    //currentPlayTime += playbackIntervalMs;

    long currentPlayTime = GameFusion::SoundDevice::Context()->currentTime();

    if (currentPlayTime > playbackEnd) {
        if (loopEnabled) {
            currentPlayTime = playbackStart;
        } else {
            stop(); // calls pause + reset
            return;
        }
    }

    // timelineView->setFrameCursorPosition(currentPlayTime);
    timeLineView->setTimeCursor(currentPlayTime);
}


