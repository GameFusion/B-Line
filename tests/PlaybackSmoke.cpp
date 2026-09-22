#include "MainWindow.h"
#include "ProjectContext.h"
#include "PaintCanvas.h"
#include "mainwindowpaint.h"
#include "../../TimeLineProject/TimeLineView.h"
#include "../../TimeLineProject/TrackItem.h"
#include "../../TimeLineProject/Track.h"
#include "../../TimeLineProject/Segment.h"
#include <QApplication>
#include <QLabel>
#include <QTreeWidget>
#include <QLineEdit>
#include <QToolButton>
#include <QMenu>
#include <QDockWidget>
#include <QScrollArea>
#include "ShotListPresentation.h"

void addShot(QTreeWidgetItem*, const GameFusion::Shot&, float);
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <QFile>
#include <QDataStream>
#include <cmath>
#include "SoundServer.h"
#include "SoundStream.h"
#include "../../TimeLineProject/AudioSegment.h"
#include <cstdio>
#include <cstdlib>

class PlaybackFixture : public MainWindow {
public:
    void prepareAnimation() {
        timeLineView->clear();timeLineView->addTrack(new Track("Shots",0,10000,TrackType::Storyboard));
        scriptBreakdown=new GameFusion::ScriptBreakdown("playback-fixture",25);
        GameFusion::Scene scene;scene.uuid="playback-scene";scene.name="Playback fixture";
        GameFusion::Shot shot;shot.uuid="playback-shot";shot.name="Motion";shot.frameCount=250;shot.startTime=0;shot.endTime=10000;
        GameFusion::Panel panel;panel.uuid="playback-panel";panel.name="Moving ink";panel.startTime=0;panel.durationTime=10000;
        GameFusion::Layer layer;layer.uuid="moving-ink";layer.name="Ink";
        GameFusion::BezierCurve curve;curve+=GameFusion::BezierControl({30,40,0},{0,0,0},{0,0,0});curve+=GameFusion::BezierControl({280,150,0},{0,0,0},{0,0,0});
        StrokeProperties props;props.foregroundColor=Qt::blue;props.maxWidth=8;curve.setStrokeProperties(props);curve.assess(20,false);layer.strokes.push_back(curve);
        GameFusion::Layer::MotionKeyFrame a,b;a.time=0;b.time=250;b.x=200;layer.motionKeyframes={a,b};
        panel.layers.push_back(layer);shot.panels.push_back(panel);scene.shots.push_back(shot);scriptBreakdown->getScenes().push_back(scene);
        updateScenes();updateTimeline();timeLineView->setTimeCursor(0L);
    }
    void detachPreview(bool enabled) { toggleDetachedPipAct->setChecked(enabled); }
};
class PaintCounter : public QObject {
public:
    int paints=0;
    bool eventFilter(QObject *,QEvent *e) override { if(e->type()==QEvent::Paint)++paints;return false; }
};
static int checks = 0;
static void require(bool condition, const char *message) {
    ++checks;
    if (!condition) { fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
static void pump(int ms) {
    QElapsedTimer timer; timer.start();
    while(timer.elapsed()<ms) {QCoreApplication::processEvents();QThread::msleep(2);}
}
static void waitForView(PaintArea *area, PaintArea::PlaybackView expected) {
    // Native activation can settle after the first scheduled playback tick.
    // Wait for the real handoff instead of assuming a fixed 100 ms deadline.
    QElapsedTimer timer;timer.start();
    while(area->playbackView()!=expected && timer.elapsed()<1000)pump(10);
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,settings.path());
    require(QSettings(QSettings::defaultFormat(),QSettings::UserScope,"B-Line","Storyboard").fileName().startsWith(settings.path()),"fixture settings are isolated from user preferences");
    ProjectContext::instance().projectJson()["fps"]=25;
    ProjectContext::instance().projectJson()["start_tc"]="01:00:00:00";
    PlaybackFixture window;
    auto *workspace=window.findChild<PaintCanvas*>();
    auto *editor=window.findChild<MainWindowPaint*>();
    QAction *embeddedLight=nullptr,*embeddedSelect=nullptr;
    for(auto *action:editor->findChildren<QAction*>()) {
        if(action->toolTip()=="Light Table Mode")embeddedLight=action;
        if(action->toolTip()=="Selection Mode")embeddedSelect=action;
    }
    auto *workspaceLight=workspace->findChild<QAction*>("workspaceLightTable");
    require(embeddedLight && embeddedSelect && workspaceLight,"both editor toolbars expose shared modes");
    workspaceLight->trigger();require(embeddedLight->isChecked(),"workspace light-table toggle synchronizes embedded button");
    embeddedLight->trigger();require(!workspaceLight->isChecked(),"embedded light-table toggle synchronizes workspace button");
    editor->getPaintArea()->setToolMode(PaintArea::ToolMode::Select);
    require(embeddedSelect->isChecked(),"shared tool changes synchronize embedded selection button");
    editor->getPaintArea()->setToolMode(PaintArea::ToolMode::Paint);
    auto *timeline=window.findChild<TimeLineView*>();
    auto *label=window.findChild<QLabel*>("playbackTimecode");
    require(timeline && label,"transport UI constructed");
    timeline->clear();
    timeline->addTrack(new Track("Test",0,1000,TrackType::Storyboard));
    window.play();
    require(!timeline->playbackActive() && label->text().contains("No sequence"),"empty timeline cannot start phantom playback");
    auto *track=timeline->getTrack(0);
    auto *segment=new Segment(timeline->scene(),0,1000);
    track->addSegment(segment);
    timeline->setTimeCursor(200L);
    window.play();
    require(timeline->playbackActive() && label->text().contains("Playing"),"Play displays active state");
    require(timeline->getCursorTime()>=200,"Play begins at cursor");
    pump(170);
    const auto advanced=timeline->getCursorTime();
    require(advanced>=320 && advanced<600,"silent project advances from monotonic clock");
    window.pause();pump(90);
    require(timeline->getCursorTime()==advanced && label->text().contains("Paused"),"Pause holds displayed frame");
    window.play();pump(80);window.pause();
    require(timeline->getCursorTime()>advanced,"Play resumes paused cursor");
    window.stop();
    require(timeline->getCursorTime()==0 && label->text().contains("Stopped"),"Stop after Pause returns to start");
    timeline->setTimeCursor(800L);window.play();pump(280);
    require(!timeline->playbackActive() && timeline->getCursorTime()==960 && label->text().contains("Ended"),"natural end holds final frame");
    window.setPlaybackLoop(true);timeline->setTimeCursor(880L);window.play();pump(210);
    require(timeline->playbackActive() && timeline->getCursorTime()<400,"Loop wraps visual clock");
    timeline->setTimeCursor(520L);pump(90);
    require(timeline->getCursorTime()>=560 && timeline->getCursorTime()<850,"scrub during playback reanchors clock");
    window.pause();
    require(label->text().contains("01:00:00:") && label->text().contains("25 fps"),"transport shows sequence timecode and frame rate");
    auto *tree=window.findChild<QTreeWidget*>("shotsTreeWidget");
    auto *filter=window.findChild<QToolButton*>("shotListFilter");
    auto *search=window.findChild<QLineEdit*>("shotListSearch");
    require(tree && filter && search, "Shot List filter and search available");
    ProjectContext::instance().setCurrentProjectPath(settings.path());
    QDir().mkpath(settings.filePath("thumbnails"));
    tree->clear();auto *scene=new QTreeWidgetItem(tree);scene->setText(0,"01 · Arrival");
    for(int i=0;i<4;++i) {
        GameFusion::Shot shot;shot.name="Shot "+std::to_string(i+1);shot.frameCount=75;shot.description="A quiet arrival at the station";
        GameFusion::Panel panel;panel.uuid="ui-fixture-"+std::to_string(i);panel.name="Panel 01";shot.panels.push_back(panel);
        QImage preview(320,180,QImage::Format_RGB32);preview.fill(QColor::fromHsv(195+i*18,70,145));
        {QPainter p(&preview);p.setPen(Qt::white);p.drawText(preview.rect(),Qt::AlignCenter,QString("Shot %1").arg(i+1));}
        preview.save(settings.filePath("thumbnails/panel_"+QString::fromStdString(panel.uuid)+".png"));
        auto *item=new QTreeWidgetItem(scene);addShot(item,shot,25);
    }
    tree->expandAll();pump(10);
    require(tree->isColumnHidden(6) && tree->isHeaderHidden(), "default Shot List hides dense metadata");
    auto *selected=scene->child(0);tree->setCurrentItem(selected);
    for(auto *a:filter->menu()->actions())if(a->text()=="Detailed")a->trigger();
    require(!tree->isColumnHidden(6) && tree->currentItem()==selected, "Detailed view retains selection and exposes metadata");
    for(auto *a:filter->menu()->actions())if(a->text()=="Thumbnails")a->trigger();
    search->setText("Shot 3");
    require(scene->child(0)->isHidden()&&!scene->child(2)->isHidden(), "shot search retains matching hierarchy");
    search->clear();
    for(auto *a:filter->menu()->actions())if(a->text()=="Show panels"){a->setChecked(true);a->trigger();}
    require(scene->child(0)->child(0)->isHidden(), "panel detail toggle works");
    for(auto *a:filter->menu()->actions())if(a->text()=="Show panels")a->trigger();
    window.resize(1500,950);window.show();pump(60);
    auto *layers=window.findChild<QDockWidget*>("dockLayers");
    auto *stroke=window.findChild<StrokeAttributeDockWidget*>();
    require(layers && stroke,"right hand docks present");
    layers->setFloating(true);layers->show();layers->resize(200,600);
    stroke->setFloating(true);stroke->show();stroke->resize(200,720);pump(60);
    fprintf(stdout,"Dock widths: Layers %d, Stroke %d\n",layers->width(),stroke->width());
    require(layers->width()<=220 && stroke->width()<=220,"Layers and Stroke Attributes resize to compact widths");
    layers->grab().save("/tmp/boarder-compact-layers.png");
    stroke->grab().save("/tmp/boarder-compact-stroke.png");
    layers->setFloating(false);stroke->setFloating(false);
    window.resizeDocks({window.findChild<QDockWidget*>("dockShots")},{320},Qt::Horizontal);
    window.resizeDocks({layers,stroke},{200,200},Qt::Horizontal);
    pump(40);

    window.grab().save("/tmp/boarder-playback-transport.png");
    // Measure actual viewport paints while the real transport animates a panel.
    window.stop();window.setPlaybackLoop(false);window.prepareAnimation();
    auto *area=editor->getPaintArea();
    PaintCounter mainPaints,workspacePaints;
    area->installEventFilter(&mainPaints);workspace->viewport()->installEventFilter(&workspacePaints);
    window.move(20,40);workspace->resize(900,600);workspace->move(500,80);
    workspace->show();workspace->raise();workspace->activateWindow();pump(100);
    window.play();pump(150);
    waitForView(area,PaintArea::PlaybackView::Workspace);
    require(area->playbackView()==PaintArea::PlaybackView::Workspace && !area->updatesEnabled(),
            "focused workspace is sole playback viewport");
    const QImage frozenMain=area->grab().toImage();
    mainPaints.paints=workspacePaints.paints=0;pump(700);
    require(workspacePaints.paints>8 && mainPaints.paints==0,"workspace playback leaves integrated paint surface frozen");
    require(label->text().contains("Render ") && label->text().contains("Target 25 fps") &&
            !label->text().contains("Render 0.0 fps"),"transport shows measured and target FPS");
    fprintf(stdout,"Workspace playback: %s; paints main=%d workspace=%d\n",qPrintable(label->text()),mainPaints.paints,workspacePaints.paints);
    require(area->grab().toImage()==frozenMain,"inactive integrated expose requests replay an unchanged snapshot");
    workspace->grab().save("/tmp/boarder-active-playback-workspace.png");
    window.grab().save("/tmp/boarder-active-playback-transport.png");
    if (app.arguments().contains("--review")) { window.setPlaybackLoop(true);pump(30000);window.stop();return 0; }
    window.raise();window.activateWindow();pump(100);
    waitForView(area,PaintArea::PlaybackView::Integrated);
    if (area->playbackView()!=PaintArea::PlaybackView::Integrated || workspace->viewport()->updatesEnabled())
        qWarning()<<"Playback focus handoff:"<<"active"<<QApplication::activeWindow()
                  <<"main"<<&window<<"workspace"<<workspace<<"view"<<int(area->playbackView())
                  <<"main visible"<<area->isVisible()<<area->visibleRegion()<<"workspace visible"<<workspace->isVisible();
    require(area->playbackView()==PaintArea::PlaybackView::Integrated && !workspace->viewport()->updatesEnabled(),
            "focusing main window transfers playback without stopping transport");
    const QImage frozenWorkspace=workspace->viewport()->grab().toImage();
    mainPaints.paints=workspacePaints.paints=0;pump(240);
    require(mainPaints.paints>2 && workspacePaints.paints==0,"main playback leaves workspace paint surface frozen");
    require(workspace->viewport()->grab().toImage()==frozenWorkspace,"inactive workspace expose requests replay an unchanged snapshot");
    workspace->raise();workspace->activateWindow();pump(80);
    workspace->showMinimized();pump(100);
    require(area->playbackView()==PaintArea::PlaybackView::Integrated,"minimized workspace falls back to visible main canvas");
    workspace->showNormal();workspace->raise();workspace->activateWindow();pump(80);
    workspace->close();pump(100);
    require(area->playbackView()==PaintArea::PlaybackView::Integrated,"closed workspace falls back to main canvas");
    window.hide();workspace->hide();pump(100);
    const auto hiddenCursor=timeline->getCursorTime();
    const auto hiddenComposite=area->compositedImage().cacheKey();
    mainPaints.paints=workspacePaints.paints=0;pump(200);
    require(area->playbackView()==PaintArea::PlaybackView::None && mainPaints.paints==0 && workspacePaints.paints==0,
            "both hidden canvases suspend drawing");
    require(timeline->getCursorTime()>hiddenCursor && area->compositedImage().cacheKey()==hiddenComposite,
            "hidden playback advances clock while deferring compositing");
    window.show();window.raise();window.activateWindow();pump(150);
    require(area->playbackView()==PaintArea::PlaybackView::Integrated && area->compositedImage().cacheKey()!=hiddenComposite,
            "revealed canvas catches up to current animated frame");
    window.pause();workspace->show();pump(200);
    mainPaints.paints=0;pump(160);
    require(mainPaints.paints<=4,"unchanged camera overlay settles without a canvas repaint loop");
    require(area->playbackView()==PaintArea::PlaybackView::All && area->updatesEnabled() && workspace->viewport()->updatesEnabled(),
            "Pause restores both editing viewports");
    window.detachPreview(true);pump(100);
    auto *preview=window.findChild<QWidget*>("detachedCameraPreview");auto *previewLabel=preview->findChild<QLabel*>();
    window.play();pump(80);
    const auto previewKey=previewLabel->pixmap().cacheKey();pump(160);
    require(previewLabel->pixmap().cacheKey()==previewKey,"detached camera preview stays frozen during playback");
    window.stop();pump(100);
    require(area->playbackView()==PaintArea::PlaybackView::All && previewLabel->pixmap().cacheKey()!=previewKey,
            "Stop restores editor and detached camera preview updates");
    window.detachPreview(false);
    area->removeEventFilter(&mainPaints);workspace->viewport()->removeEventFilter(&workspacePaints);

    if (app.arguments().contains("--audio")) {
        const QString wav=settings.filePath("transport-tone.wav");
        QFile file(wav);require(file.open(QIODevice::WriteOnly),"audio fixture opens");
        QDataStream out(&file);out.setByteOrder(QDataStream::LittleEndian);
        const int samples=44100*2,bytes=samples*4;
        out.writeRawData("RIFF",4);out<<quint32(bytes+36);out.writeRawData("WAVEfmt ",8);out<<quint32(16)<<quint16(1)<<quint16(2)<<quint32(44100)<<quint32(176400)<<quint16(4)<<quint16(16);out.writeRawData("data",4);out<<quint32(bytes);
        for(int i=0;i<samples;++i){qint16 value=qint16(1800*std::sin(i*2*3.141592653589793*440/44100));out<<value<<value;}file.close();
        auto *audio=GameFusion::SoundServer::Initialize(44100,2);require(audio,"native audio device opens");
        auto *audioTrack=timeline->addTrack(new Track("Tone",0,2000,TrackType::Audio));
        auto *clip=new AudioSegment(timeline->scene());clip->loadAudio("Tone",wav.toUtf8().constData());audioTrack->addSegment(clip);
        window.stop();window.setPlaybackLoop(false);window.play();pump(220);
        require(audio->isPlaying() && (*audio)[0]==audioTrack->getSoundStream(),"Play attaches timeline audio without prior scrubbing");
        require(audioTrack->getSoundStream()->position()>0,"native audio queue consumes samples");
        fprintf(stdout,"Native audio meter: %.2f dB\n",audio->getDecibelsLeft());
        window.pause();require(!audio->isPlaying(),"Pause stops native audio queue");
        window.stop();audio->clearLayers();
    }
    window.hide();
    fprintf(stdout,"PlaybackSmoke: PASS (%d checks)\n",checks);
    return 0;
}
