// Compare a culled timeline header against the original full-range renderer.
#include "../../TimeLineProject/TimeLineView.h"
#include <QApplication>
#include <QImage>
#include <QEventLoop>
#include <QPaintEvent>
#include <QTimer>
#include <QPainter>
#include <QSettings>
#include <QStyleOptionGraphicsItem>
#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>

class PaintRegions : public QObject {
public:
    int widest=0,paints=0;
    bool eventFilter(QObject *,QEvent *e) override {
        if(e->type()==QEvent::Paint){++paints;widest=qMax(widest,static_cast<QPaintEvent*>(e)->region().boundingRect().width());}
        return false;
    }
};
static void pump(int ms) {
    QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();
}
static void fullHeader(QPainter &p, HeaderItem &header, TimeLineView &view, float scale) {
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setBrush(QColor(0,0,0,150));p.setPen(Qt::NoPen);p.drawRect(header.boundingRect());
    p.setPen(Qt::white);
    for(int ms=0;ms<header.boundingRect().width()/scale;ms+=100) {
        const float x=ms*scale;p.drawLine(x,0,x,ms%500==0?10:5);
    }
    for(int ms=0;ms<header.boundingRect().width()/scale;ms+=100) {
        if(scale>=2 && (ms/100+1)%2==0)continue;
        const qreal x=ms*scale+2;
        if(ms%500==0)p.drawText(x,20,view.getTimeFormat()==TimeLineView::Milliseconds?
                               QString::number(ms):view.formatTime(ms/1000.));
    }
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,settings.path());
    TimeLineView view;
    auto *header=new HeaderItem;view.scene()->addItem(header);header->setTimelineWidth(60000);
    int checks=0;
    for(float scale:{0.1f,0.35f,1.f,2.5f}) {
        header->setScale(scale);
        for(auto format:{TimeLineView::Milliseconds,TimeLineView::Seconds,TimeLineView::Frames,TimeLineView::Timecode}) {
            view.setTimeDisplayFormat(format);
            for(qreal left:{0.,37.,500.*scale+8,3037.,59900.}) {
                const QRectF exposed(left,0,400,30);
                auto render=[&](bool culled){
                    QImage image(800,60,QImage::Format_ARGB32_Premultiplied);image.fill(QColor(30,40,50));
                    QPainter p(&image);p.scale(2,2);p.translate(-left,0);p.setClipRect(exposed);
                    if(culled){QStyleOptionGraphicsItem option;option.exposedRect=exposed;header->paint(&p,&option);}
                    else fullHeader(p,*header,view,scale);
                    p.end();return image;
                };
                ++checks;
                const auto expected=render(false),actual=render(true);
                if(expected!=actual){
                    expected.save("/tmp/boarder-header-expected.png");actual.save("/tmp/boarder-header-actual.png");
                    fprintf(stderr,"FAIL: header differs at scale=%g format=%d left=%g\n",scale,int(format),left);return 1;
                }
            }
        }
    }
    delete header;
    view.resize(900,260);view.show();view.setTimeCursor(8L);pump(100);
    PaintRegions regions;view.viewport()->installEventFilter(&regions);
    view.setTimeCursor(9L);pump(100);++checks;
    if(regions.paints==0 || regions.widest>=view.viewport()->width()/2){
        fprintf(stderr,"FAIL: cursor repaints full timeline (paints=%d width=%d viewport=%d)\n",
                regions.paints,regions.widest,view.viewport()->width());return 1;
    }
    printf("TimelinePaintSmoke: PASS (%d checks, cursor update width=%d / %d)\n",checks,regions.widest,view.viewport()->width());
}
