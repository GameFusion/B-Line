// Synthetic input-to-Qt-paint benchmark; does not measure hardware/display latency.
#include "PaintCanvas.h"
#include "Worker.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QDataStream>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

static QJsonObject distribution(std::vector<double> values) {
    if (values.empty()) return {{"count",0}};
    std::sort(values.begin(),values.end());
    auto percentile=[&](double p){return values[std::min(values.size()-1,size_t(std::ceil(p*values.size())-1))];};
    return {{"count",int(values.size())},{"p50_ms",percentile(.5)},
            {"p95_ms",percentile(.95)},{"max_ms",values.back()}};
}
static void report(QJsonObject object) {
    puts(QJsonDocument(object).toJson(QJsonDocument::Compact).constData()); fflush(stdout);
}
static QPointF point(int i) {
    return {80+480*(.5+.5*std::sin(i*.013)),230+140*std::sin(i*.037)};
}
static StrokeProperties properties() {
    StrokeProperties props; props.minWidth=2; props.maxWidth=8;
    props.foregroundColor=Qt::black; props.variableWidthMode=StrokeProperties::Pressure;
    props.stepCount=20; return props;
}
static QString fingerprint(const GameFusion::BezierCurve &curve) {
    QByteArray bytes; QDataStream stream(&bytes,QIODevice::WriteOnly);
    for(const auto &c:curve) stream<<c.point.x()<<c.point.y()<<c.leftControl.x()<<c.leftControl.y()
        <<c.rightControl.x()<<c.rightControl.y()<<c.indexPoint;
    for(const auto &v:curve.vertexArray()) stream<<v.x()<<v.y();
    for(float p:curve.strokePressure())stream<<p;
    return QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex();
}
static void fitBenchmark() {
    const auto props=properties();
    for(int n:{64,256,1024}) {
        std::vector<GameFusion::StrokePoint> points;
        for(int i=0;i<n;++i)points.push_back({point(i),float(.5+.45*std::sin(i*.019))});
        std::vector<double> times; FitResult fit;
        for(int repeat=0;repeat<3;++repeat){QElapsedTimer timer;timer.start();performLocalFit(fit,points,props);times.push_back(timer.nsecsElapsed()/1e6);}
        report({{"case","full-fit"},{"points",n},{"fit",distribution(times)},{"sha256",fingerprint(fit.curve)}});
    }
    // Reproduce growing previews in the actual Worker, including pressure mapping.
    Worker worker("ink",0,0,props); std::vector<double> times;
    QString hash;int covered=0;
    QObject::connect(&worker,&Worker::resultReady,&worker,[&](const WorkerResults &result,bool){hash=fingerprint(result.curve);covered=result.pointIndex;});
    for(int i=0;i<1024;++i){worker.addPoint(point(i),float(.5+.45*std::sin(i*.019)));
        if((i+1)%4==0){QElapsedTimer timer;timer.start();worker.requestCompletion();times.push_back(timer.nsecsElapsed()/1e6);}}
    report({{"case","growing-worker"},{"points",covered},{"fit",distribution(times)},{"sha256",hash}});
}
static void fitChecks() {
    int checks=0;
    for(int shape=0;shape<5;++shape)for(int stride:{1,7,37}) {
        StrokeFitCache cache; std::vector<GameFusion::StrokePoint> points;
        auto props=properties();props.stepCount=shape%2?7:20;
        for(int i=0;i<180;++i){
            QPointF pos=point(i);
            if(shape==1)pos={double(i),double(i%20<10?i%10:10-i%10)};
            if(shape==2)pos={12,12};
            if(shape==3)pos={i*.25,-i*.25};
            if(shape==4)pos={-100+80*std::cos(i*.31),-100+80*std::sin(i*.31)};
            points.push_back({pos,float(.5+.45*std::sin(i*.019))});
            if(i%stride && i!=179)continue;
            FitResult full,incremental;
            performLocalFit(full,points,props);performLocalFit(incremental,points,props,&cache);
            if(fingerprint(full.curve)!=fingerprint(incremental.curve)){
                fprintf(stderr,"FAIL: fit mismatch shape=%d stride=%d points=%d\n",shape,stride,i+1);std::exit(1);
            }
            ++checks;
        }
        // A large queued batch and cache reset must also agree exactly.
        for(int i=180;i<2048;++i)points.push_back({point(i),float(.5+.45*std::sin(i*.019))});
        for(bool reset:{false,true}){
            if(reset)cache=StrokeFitCache();
            FitResult full,incremental;performLocalFit(full,points,props);performLocalFit(incremental,points,props,&cache);
            if(fingerprint(full.curve)!=fingerprint(incremental.curve)){fprintf(stderr,"FAIL: batched fit mismatch\n");std::exit(1);}++checks;
        }
    }
    report({{"case","fit-equivalence"},{"checks",checks},{"result","pass"}});
}
struct Samples {
    QElapsedTimer clock;
    bool collecting=false;
    size_t painted=0;
    int paints=0;
    std::vector<double> inputs,latencies,paintTimes,handlers,intervals;
    void didPaint(double start) {
        if(!collecting)return;
        ++paints; const double end=clock.nsecsElapsed()/1e6; paintTimes.push_back(end-start);
        while(painted<inputs.size())latencies.push_back(end-inputs[painted++]);
    }
};
class MeasuredArea:public PaintArea {
public: Samples *samples=nullptr;
protected:void paintEvent(QPaintEvent *event)override {
    const double start=samples?samples->clock.nsecsElapsed()/1e6:0;
    PaintArea::paintEvent(event);if(samples)samples->didPaint(start);
}};
class MeasuredCanvas:public PaintCanvas {
public: Samples *samples=nullptr;
protected:void paintEvent(QPaintEvent *event)override {
    const double start=samples?samples->clock.nsecsElapsed()/1e6:0;
    PaintCanvas::paintEvent(event);if(samples)samples->didPaint(start);
}};
static GameFusion::BezierCurve backgroundLine(int i) {
    GameFusion::BezierCurve curve;
    curve+=GameFusion::BezierControl({float(30+i%25*23),40,0},{0,0,0},{70,80,0});
    curve+=GameFusion::BezierControl({float(30+i%25*23),440,0},{-70,-80,0},{0,0,0});
    auto props=properties();props.foregroundColor=QColor(170,180,190);props.maxWidth=2;
    curve.setStrokeProperties(props);curve.assess(20,false);curve.setStrokePressure(std::vector<float>(curve.vertexArray().size(),.5));return curve;
}
static void uiBenchmark(QApplication &app,bool workspace,bool dense,bool rapid) {
    MeasuredArea area;area.setDimensions(640,480,640,480);area.resize(640,480);
    area.toggleOutputFrame(false);area.toggleActionSafe(false);area.toggleTitleSafe(false);area.setPipDisplay(false);
    GameFusion::Panel panel;panel.uuid="latency";GameFusion::Layer ink;ink.uuid="ink";panel.layers.push_back(ink);
    if(dense){GameFusion::Layer background;background.uuid="background";for(int i=0;i<300;++i)background.strokes.push_back(backgroundLine(i));panel.layers.push_back(background);}
    area.setPanel(panel);area.setActiveLayer("ink");area.setStrokeProperties(properties());area.setLightTableMode(dense);
    area.setToolMode(PaintArea::ToolMode::Paint);
    MeasuredCanvas view;view.resize(800,600);view.setPaintArea(&area);
    // Only show the measured window: native occlusion otherwise suppresses paints.
    if(workspace){view.show();view.activateWindow();view.setFocus();}else{area.show();area.activateWindow();area.setFocus();}
    QEventLoop settle;QTimer::singleShot(100,&settle,&QEventLoop::quit);settle.exec();view.fitToBase();app.processEvents();
    Samples samples;samples.clock.start();samples.collecting=true;
    if(workspace)view.samples=&samples;else area.samples=&samples;
    std::vector<double> commits,releases;int completed=0,accepted=0;
    QObject::connect(&area,&PaintArea::newPointAvailable,&area,[&]{++accepted;});
    QObject::connect(&area,&PaintArea::layerModified,&area,[&](const GameFusion::Layer &layer){
        if(int(layer.strokes.size())>completed){completed=int(layer.strokes.size());
            if(completed<=int(releases.size()))commits.push_back(samples.clock.nsecsElapsed()/1e6-releases[completed-1]);}
    });
    auto send=[&](QEvent::Type type,int i){
        const QPointF pos=workspace?view.mapFromScene(point(i)):point(i)*area.zoomFactor();
        QWidget *target=workspace?view.viewport():&area;
        const double start=samples.clock.nsecsElapsed()/1e6;
        if(type!=QEvent::MouseButtonRelease){
            if(!samples.inputs.empty())samples.intervals.push_back(start-samples.inputs.back());
            samples.inputs.push_back(start);
        }else releases.push_back(start);
        QMouseEvent event(type,pos,target->mapToGlobal(pos.toPoint()),type==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton,
                          type==QEvent::MouseButtonRelease?Qt::NoButton:Qt::LeftButton,Qt::NoModifier);
        QCoreApplication::sendEvent(target,&event);samples.handlers.push_back(samples.clock.nsecsElapsed()/1e6-start);
    };
    const int pointsPerStroke=rapid?32:1024,strokes=rapid?12:1;
    for(int stroke=0;stroke<strokes;++stroke){
        send(QEvent::MouseButtonPress,0);
        QEventLoop loop;QTimer input;input.setTimerType(Qt::PreciseTimer);int i=1;
        QObject::connect(&input,&QTimer::timeout,&loop,[&]{
            if(i<pointsPerStroke){send(QEvent::MouseMove,i++);}
            else {input.stop();send(QEvent::MouseButtonRelease,i-1);loop.quit();}
        });input.start(4);loop.exec();
        // Preserve fast successive strokes; do not wait for final fitting here.
    }
    QElapsedTimer deadline;deadline.start();
    auto fitting=[&]{for(auto *thread:area.findChildren<QThread*>())if(thread->isRunning())return true;return false;};
    while((completed<strokes || fitting()) && deadline.elapsed()<5000){app.processEvents();QThread::msleep(1);}
    if(completed!=strokes || fitting()){
        report({{"error","stroke completion timeout"},{"completed",completed},{"accepted",accepted},{"submitted",int(samples.inputs.size())}});std::_Exit(1);
    }
    area.finishPendingStrokes();app.processEvents();samples.collecting=false;
    report({{"case","input-to-paint"},{"view",workspace?"workspace":"integrated"},{"background",dense?"300-stroke-light-table":"empty"},
            {"gesture",rapid?"12-short-strokes":"1024-point-stroke"},{"input",distribution(samples.intervals)},
            {"handler",distribution(samples.handlers)},{"input_to_paint",distribution(samples.latencies)},
            {"paint",distribution(samples.paintTimes)},{"release_to_commit",distribution(commits)},
            {"submitted",int(samples.inputs.size())},{"accepted",accepted},{"painted_samples",int(samples.painted)},{"completed",completed}});
    if(accepted!=int(samples.inputs.size()) || samples.painted!=samples.inputs.size()){
        fprintf(stderr,"FAIL: input interrupted or window occluded; reject this timing run\n");std::exit(1);
    }
    view.samples=nullptr;area.samples=nullptr;
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    report({{"environment",QGuiApplication::platformName()},{"qt",qVersion()},{"dpr",app.devicePixelRatio()}});
    if(app.arguments().contains("--verify"))fitChecks();
    else if(app.arguments().contains("--fit"))fitBenchmark();
    else if(app.arguments().contains("--ui"))uiBenchmark(app,app.arguments().contains("--workspace"),app.arguments().contains("--dense"),app.arguments().contains("--rapid"));
    else for(bool workspace:{false,true})for(bool dense:{false,true})for(bool rapid:{true,false})uiBenchmark(app,workspace,dense,rapid);
}
