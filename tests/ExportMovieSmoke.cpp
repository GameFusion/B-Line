#include "MainWindow.h"
#include "ProjectContext.h"
#include "../../TimeLineProject/TimeLineView.h"
#include "../../TimeLineProject/Track.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QSettings>
#include <QJsonArray>
#include <QDirIterator>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include <QThread>
#include <cstdio>
#include <cstdlib>
class ExportFixture : public MainWindow {
public:
    void prepare(const QString &directory) {
        auto &project=ProjectContext::instance();project.setCurrentProjectPath(directory);project.setCurrentProjectName("Short review");
        project.projectJson()["fps"]=25;project.projectJson()["resolution"]=QJsonArray{320,180};project.projectJson()["canvas"]=QJsonArray{320,180};
        timeLineView->clear();timeLineView->addTrack(new Track("Shots",0,200,TrackType::Storyboard));
        scriptBreakdown=new GameFusion::ScriptBreakdown("fixture",25);
        GameFusion::Scene scene;scene.uuid="export-scene";scene.name="Export fixture";
        GameFusion::Shot shot;shot.uuid="export-shot";shot.name="Shot 01";shot.frameCount=5;shot.startTime=0;shot.endTime=200;
        GameFusion::Panel panel;panel.uuid="export-panel";panel.name="Panel 01";panel.startTime=0;panel.durationTime=200;
        GameFusion::Layer layer;layer.uuid="export-ink";layer.name="Ink";
        GameFusion::BezierCurve curve;curve+=GameFusion::BezierControl({30,40,0},{0,0,0},{0,0,0});curve+=GameFusion::BezierControl({280,150,0},{0,0,0},{0,0,0});
        StrokeProperties props;props.foregroundColor=Qt::blue;props.maxWidth=8;curve.setStrokeProperties(props);curve.assess(20,false);layer.strokes.push_back(curve);panel.layers.push_back(layer);shot.panels.push_back(panel);scene.shots.push_back(shot);scriptBreakdown->getScenes().push_back(scene);
        updateScenes();updateTimeline();timeLineView->setTimeCursor(80L);
    }
    double cursor() {return timeLineView->getCursorTime();}
};
static void require(bool ok,const char *message){if(!ok){fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main(int argc,char **argv){
    QApplication app(argc,argv);QTemporaryDir dir;
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());
    ExportFixture editor;editor.prepare(dir.path());editor.exportMovie();
    QDirIterator files(dir.path(),{"*.mp4"},QDir::Files,QDirIterator::Subdirectories);
    require(files.hasNext(),"Export Movie creates an actual MP4");const QString path=files.next();require(QFileInfo(path).size()>0,"movie is nonempty");
    auto *player=editor.findChild<QWidget*>("exportMoviePlayer");require(player && player->isVisible(),"successful export opens player window");
    require(editor.cursor()==80,"export restores editing cursor");
    auto *media=player->findChild<QMediaPlayer*>();QElapsedTimer timer;timer.start();
    while(media->duration()==0&&timer.elapsed()<6000){app.processEvents();QThread::msleep(5);}
    require(qAbs(media->duration()-200)<45,"five frames at 25 fps retain duration");
    fprintf(stdout,"ExportMovieSmoke: PASS (render, encode, silent project, player, cursor, duration)\n");
    return 0;
}
