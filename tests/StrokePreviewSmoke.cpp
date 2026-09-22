#include "paintarea.h"
#include "Worker.h"
#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <QCryptographicHash>
#include <QDataStream>
#include <cstdio>
#include <cstdlib>
#include <cmath>

static int checks=0;
static void require(bool ok,const char *message){++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static QByteArray geometry(const GameFusion::BezierCurve &curve){
    QByteArray bytes;QDataStream out(&bytes,QIODevice::WriteOnly);
    for(const auto &v:curve.vertexArray())out<<v.x()<<v.y();
    for(float p:curve.strokePressure())out<<p;
    return QCryptographicHash::hash(bytes,QCryptographicHash::Sha256);
}
static int difference(const QImage &a,const QImage &b){
    int delta=0;
    for(int y=0;y<a.height();++y)for(int x=0;x<a.width();++x){
        const auto p=a.pixelColor(x,y),q=b.pixelColor(x,y);
        delta=qMax(delta,qAbs(p.red()-q.red()));delta=qMax(delta,qAbs(p.green()-q.green()));
        delta=qMax(delta,qAbs(p.blue()-q.blue()));delta=qMax(delta,qAbs(p.alpha()-q.alpha()));
    }
    return delta;
}
int main(int argc,char **argv){
    QApplication app(argc,argv);int maximum=0;
    for(auto color:{StrokeProperties::SolidForeground,StrokeProperties::GradientFGtoBG})
    for(auto width:{StrokeProperties::Uniform,StrokeProperties::Pressure,StrokeProperties::TaperOut}){
        StrokeProperties props;props.variableWidthMode=width;props.colorMode=color;props.minWidth=2;props.maxWidth=11;
        props.foregroundColor=QColor(220,30,60,220);props.backgroundColor=QColor(40,80,220,160);props.stepCount=20;
        Worker worker("ink",0,0,props);WorkerResults result;int results=0;
        QObject::connect(&worker,&Worker::resultReady,&worker,[&](const WorkerResults &r,bool){result=r;++results;});
        StrokePreviewTarget initial{QRectF(-40,-30,480,360),1,2};worker.setPreviewTarget(initial);worker.process();
        for(int i=0;i<140;++i)worker.addPoint({170+150*std::sin(i*.049),140+100*std::sin(i*.12)},.5+.45*std::sin(i*.067));
        QEventLoop loop;QTimer::singleShot(100,&loop,&QEventLoop::quit);loop.exec();
        require(!result.preview.isNull(),"long fitted stroke has a worker-rendered preview");
        const auto original=geometry(result.curve);
        auto moved=initial;moved.bounds.translate(-71,23);worker.setPreviewTarget(moved);
        require(results==1 && result.previewTarget==initial,"unacknowledged preview prevents another queued bitmap");
        worker.previewConsumed();require(results==2 && result.previewTarget==moved,"acknowledgment catches up a stationary pen after pan");
        for(double zoom:{.75,1.,1.6})for(qreal density:{1.,2.}){
            PaintArea area;area.setDimensions(480,360,400,300);
            GameFusion::Panel panel;panel.uuid="preview";GameFusion::Layer ink;ink.uuid="ink";panel.layers={ink};area.setPanel(panel);
            area.zoomIn(zoom);area.setToolMode(PaintArea::ToolMode::Select);
            const QRectF visible(-70,35,340,250);const QPointF margin(40*area.zoomFactor(),30*area.zoomFactor());
            const StrokePreviewTarget target{visible.translated(-margin),area.zoomFactor(),density};
            worker.previewConsumed();worker.setPreviewTarget(target);
            require(!result.preview.isNull() && result.previewTarget==target,"zoom and density produce a matching preview");
            require(geometry(result.curve)==original,"viewport changes leave fitted geometry and pressure untouched");
            require(QMetaObject::invokeMethod(&area,"handleResult",Qt::DirectConnection,Q_ARG(WorkerResults,result),Q_ARG(bool,false)),"inject fitted result through normal receiver");
            auto render=[&](bool cached){
                QImage image(QSize(340*density,250*density),QImage::Format_ARGB32_Premultiplied);image.fill(QColor(35,70,105));
                QPainter painter(&image);painter.setRenderHints(QPainter::Antialiasing|QPainter::SmoothPixmapTransform);
                painter.scale(density,density);painter.translate(-visible.x(),-visible.y());
                area.drawLiveStroke(painter,visible,density,cached);painter.end();return image;
            };
            const auto expected=render(false),actual=render(true);const int delta=difference(expected,actual);maximum=qMax(maximum,delta);
            if(delta>3){expected.save("/tmp/boarder-stroke-reference.png");actual.save("/tmp/boarder-stroke-preview.png");fprintf(stderr,"pixel delta=%d color=%d width=%d zoom=%g dpr=%g\n",delta,int(color),int(width),zoom,density);}
            require(delta<=3,"cached preview agrees with direct drawing within 8-bit alpha rounding");
            const QRectF changed=visible.translated(80,20);
            auto mismatch=[&](bool cached){QImage image(600,500,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::white);QPainter p(&image);area.drawLiveStroke(p,changed,density,cached);p.end();return image;};
            require(mismatch(true)==mismatch(false),"stale viewport preview falls back to the complete vector curve");
        }
        worker.previewConsumed();worker.setPreviewTarget({QRectF(0,0,100000,100000),1,2});
        require(result.preview.isNull(),"oversized preview target uses vector fallback without allocating huge images");
        worker.requestCompletion();require(geometry(result.curve)==original,"final completion retains fitted geometry and pressure");
    }
    {
        StrokeProperties props;props.variableWidthMode=StrokeProperties::Pressure;props.stepCount=20;
        Worker worker("busy",0,0,props);WorkerResults result;int results=0;
        QObject::connect(&worker,&Worker::resultReady,&worker,[&](const WorkerResults &r,bool){result=r;++results;});
        StrokePreviewTarget target{QRectF(0,0,480,360),1,2};worker.setPreviewTarget(target);worker.process();
        auto settle=[] {QEventLoop loop;QTimer::singleShot(100,&loop,&QEventLoop::quit);loop.exec();};
        for(int i=0;i<140;++i)worker.addPoint({100+i*1.5,140+60*std::sin(i*.12)},.5);
        settle();require(!result.preview.isNull(),"busy GUI fixture has an unacknowledged preview");
        target.bounds.translate(21,-17);worker.setPreviewTarget(target);
        for(int i=140;i<160;++i)worker.addPoint({100+i*1.5,140+60*std::sin(i*.12)},.8);
        settle();require(results==2 && result.preview.isNull() && result.previewTarget==target,
                         "a newer fit under backpressure delivers geometry without another bitmap");
        const auto latest=geometry(result.curve);
        worker.previewConsumed();
        require(results==3 && !result.preview.isNull() && result.previewTarget==target,
                "acknowledgment rasterizes the latest fit and target without another pen event");
        require(geometry(result.curve)==latest,"backpressure catch-up keeps the latest geometry unchanged");
        worker.requestCompletion();
    }
    printf("StrokePreviewSmoke: PASS (%d checks, maximum channel rounding=%d)\n",checks,maximum);
}
