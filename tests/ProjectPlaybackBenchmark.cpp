// Use a disposable full copy of a project: the normal loader may write caches.
#include "MainWindow.h"
#include "PaintCanvas.h"
#include "ProjectContext.h"
#include "mainwindowpaint.h"
#include "../../TimeLineProject/TimeLineView.h"
#include "SoundServer.h"
#include "SoundStream.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>

static void report(const QJsonObject &value) {
    puts(QJsonDocument(value).toJson(QJsonDocument::Compact).constData());fflush(stdout);
}
static QJsonObject stats(std::vector<double> values) {
    if(values.empty())return {{"count",0}};
    double sum=0;for(double v:values)sum+=v;std::sort(values.begin(),values.end());
    auto p=[&](double f){return values[std::min(values.size()-1,size_t(std::ceil(values.size()*f)-1))];};
    return {{"count",int(values.size())},{"mean_ms",sum/values.size()},
            {"p50_ms",p(.5)},{"p95_ms",p(.95)},{"max_ms",values.back()},{"total_ms",sum}};
}
static void pump(int ms) {
    QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();
}
class MeasuredApplication:public QApplication {
public:
    using QApplication::QApplication;
    bool measuring=false;
    QObject *integrated=nullptr,*workspace=nullptr,*timeline=nullptr;
    std::vector<double> integratedPaints,workspacePaints,timelinePaints;
    bool notify(QObject *object,QEvent *event)override {
        std::vector<double> *samples=nullptr;
        if(measuring && event->type()==QEvent::Paint){
            if(object==integrated)samples=&integratedPaints;
            if(object==workspace)samples=&workspacePaints;
            if(object==timeline)samples=&timelinePaints;
        }
        QElapsedTimer elapsed;if(samples)elapsed.start();
        const bool handled=QApplication::notify(object,event);
        if(samples)samples->push_back(elapsed.nsecsElapsed()/1e6);
        return handled;
    }
};
class ProjectFixture:public MainWindow {
public:
    bool measuring=false;
    std::vector<double> ticks,transitions;
    ProjectFixture() {
        disconnect(playbackTimer,&QTimer::timeout,this,&MainWindow::onPlaybackTick);
        connect(playbackTimer,&QTimer::timeout,this,[this]{
            QElapsedTimer elapsed;elapsed.start();auto *previous=currentPanel;
            onPlaybackTick();
            if(measuring){double ms=elapsed.nsecsElapsed()/1e6;ticks.push_back(ms);if(previous!=currentPanel)transitions.push_back(ms);}
        });
    }
    TimeLineView *timeline()const{return timeLineView;}
    long sequenceDuration()const{return episodeDuration.durationMs;}
    bool playing()const{return isPlaying;}
    void seek(long ms){timeLineView->setTimeCursor(ms);}
    QString panelId()const{return currentPanel?QString::fromStdString(currentPanel->uuid):QString();}
    int panelCount()const{int n=0;for(auto &s:scriptBreakdown->getScenes())for(auto &shot:s.shots)n+=shot.panels.size();return n;}
};
int main(int argc,char **argv) {
    MeasuredApplication app(argc,argv);const QStringList args=app.arguments();
    auto argument=[&](const QString &name,const QString &fallback){int i=args.indexOf(name);return i>=0&&i+1<args.size()?args[i+1]:fallback;};
    const QString project=argument("--project",{});
    if(!args.contains("--disposable-copy") || !QFileInfo::exists(project+"/project.json")){
        fprintf(stderr,"Use --project PATH --disposable-copy, optionally --workspace, --audio, --start-ms N, --duration-ms N.\n");return 2;
    }
    QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,settings.path());
    if(!QSettings(QSettings::IniFormat,QSettings::UserScope,"B-Line","Storyboard").fileName().startsWith(settings.path()))return 3;
    if(args.contains("--audio"))GameFusion::SoundServer::Initialize(44100,2);
    ProjectFixture editor;editor.resize(1440,900);editor.show();pump(150);
    QElapsedTimer loading;loading.start();editor.loadProject(project);
    report({{"phase","loaded"},{"load_ms",loading.elapsed()},{"sequence_ms",qint64(editor.sequenceDuration())},
            {"panels",editor.panelCount()},{"qt",qVersion()},{"platform",QGuiApplication::platformName()},{"dpr",app.devicePixelRatio()}});
    auto *area=editor.findChild<MainWindowPaint*>()->getPaintArea();
    auto *workspace=editor.findChild<PaintCanvas*>();auto *timeline=editor.timeline();
    const bool standalone=args.contains("--workspace");
    const long start=argument("--start-ms","0").toLong();
    const int duration=argument("--duration-ms",QString::number(editor.sequenceDuration()-start)).toInt();
    editor.seek(start);editor.setPlaybackLoop(false);
    if(standalone){workspace->resize(1200,800);workspace->show();workspace->raise();workspace->activateWindow();workspace->fitToBase();}
    else {workspace->hide();editor.raise();editor.activateWindow();}
    pump(600);
    app.integrated=area;app.workspace=workspace->viewport();app.timeline=timeline->viewport();
    QElapsedTimer clock;clock.start();std::vector<double> intervals;
    std::set<qint64> frames;std::set<QString> panels;double previousPaint=-1;
    const double fps=ProjectContext::instance().projectJson()["fps"].toDouble();
    int wrongView=0,composites=0;
    QObject::connect(area,&PaintArea::compositImageModified,&editor,[&]{if(editor.measuring)++composites;});
    QObject::connect(area,&PaintArea::playbackFramePainted,&editor,[&](quint64){
        if(!editor.measuring)return;
        if(area->playbackView()!=(standalone?PaintArea::PlaybackView::Workspace:PaintArea::PlaybackView::Integrated))++wrongView;
        const auto frame=qRound64(timeline->getCursorTime()*fps/1000);
        if(!frames.insert(frame).second)return;
        const double now=clock.nsecsElapsed()/1e6;if(previousPaint>=0)intervals.push_back(now-previousPaint);previousPaint=now;
        panels.insert(editor.panelId());
    });
    report({{"phase","measuring"},{"view",standalone?"workspace":"integrated"},{"target_fps",fps},{"start_ms",qint64(start)},{"duration_ms",duration}});
    clock.restart();editor.measuring=app.measuring=true;editor.play();
    QEventLoop run;QTimer deadline;deadline.setSingleShot(true);deadline.setTimerType(Qt::PreciseTimer);
    QObject::connect(&deadline,&QTimer::timeout,&run,&QEventLoop::quit);
    QTimer ended;QObject::connect(&ended,&QTimer::timeout,&run,[&]{if(!editor.playing())run.quit();});
    deadline.start(duration);ended.start(25);run.exec();
    editor.measuring=app.measuring=false;const double elapsed=clock.nsecsElapsed()/1e6;
    const double cursor=timeline->getCursorTime();
    const QString display=editor.findChild<QLabel*>("playbackTimecode")->text();
    auto *audio=GameFusion::SoundServer::context();
    const bool audioPlaying=audio && audio->isPlaying();
    auto *stream=audio ? (*audio)[0] : nullptr;
    const qint64 audioPosition=stream ? stream->position() : 0;
    editor.pause();
    report({{"phase","result"},{"view",standalone?"workspace":"integrated"},{"target_fps",fps},
            {"wall_ms",elapsed},{"cursor_ms",cursor},{"unique_frames",int(frames.size())},{"painted_panels",int(panels.size())},
            {"render_fps",frames.size()*1000/elapsed},{"frame_interval",stats(intervals)},{"tick",stats(editor.ticks)},
            {"panel_transition",stats(editor.transitions)},{"main_paint",stats(app.integratedPaints)},
            {"workspace_paint",stats(app.workspacePaints)},{"timeline_paint",stats(app.timelinePaints)},
            {"editing_composite_notifications",composites},{"wrong_view_frames",wrongView},{"transport",display},
            {"audio_requested",args.contains("--audio")},{"audio_playing",audioPlaying},{"audio_stream_position",audioPosition}});
    if(wrongView || frames.empty()){fprintf(stderr,"FAIL: viewport focus/visibility invalidated the measurement\n");return 1;}
    workspace->hide();editor.hide();
    return 0;
}
