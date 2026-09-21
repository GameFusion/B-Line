#include "MoviePlayerWindow.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include <QPushButton>
#include <QClipboard>
#include <QMimeData>
#include <QElapsedTimer>
#include <QThread>
#include <cstdio>
#include <cstdlib>
static void require(bool ok,const char *message){if(!ok){fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static void pump(int ms){QElapsedTimer t;t.start();while(t.elapsed()<ms){QCoreApplication::processEvents();QThread::msleep(2);}}
int main(int argc,char **argv){
    QApplication app(argc,argv);QTemporaryDir dir;
    for(int i=0;i<25;++i){QImage frame(320,180,QImage::Format_RGB32);frame.fill(QColor(20+i*3,60,90));QPainter p(&frame);p.setPen(Qt::white);p.drawText(frame.rect(),Qt::AlignCenter,QString("Movie review · frame %1").arg(i));p.end();require(frame.save(dir.filePath(QString("frame_%1.png").arg(i,5,10,QChar('0')))),"fixture frame saved");}
    QString error;const QString movie=dir.filePath("Review with spaces å.mp4");
    require(MoviePlayerWindow::encode(dir.path(),{},movie,25,1,nullptr,&error),qPrintable(error));
    MoviePlayerWindow player(movie);player.show();
    auto *media=player.findChild<QMediaPlayer*>();require(media,"native media player present");
    int frames=0;QObject::connect(media->videoSink(),&QVideoSink::videoFrameChanged,&player,[&](const QVideoFrame &frame){if(frame.isValid())++frames;});
    QElapsedTimer wait;wait.start();while((!frames||media->duration()==0)&&wait.elapsed()<6000)pump(20);
    require(frames>0 && media->hasVideo(),"encoded MP4 decodes actual video frames");
    require(qAbs(media->duration()-1000)<80,"movie duration matches project frame rate");
    media->pause();media->setPosition(600);pump(180);
    require(qAbs(media->position()-600)<120,"player seeks while paused");
    auto *copy=player.findChild<QPushButton*>("movieCopyPath");auto *reveal=player.findChild<QPushButton*>("movieReveal");require(copy && reveal && reveal->isEnabled(),"Copy Path and Reveal actions available");
    auto *saved=new QMimeData;const auto *clipboard=QApplication::clipboard()->mimeData();if(clipboard)for(const auto &format:clipboard->formats())saved->setData(format,clipboard->data(format));
    copy->click();const bool copied=QApplication::clipboard()->text()==QDir::toNativeSeparators(movie);QApplication::clipboard()->setMimeData(saved);require(copied,"Copy Path preserves absolute Unicode path");
    player.grab().save("/tmp/boarder-movie-player.png");
    require(!media->videoSink()->videoFrame().toImage().isNull(),"paused player retains actual video pixels");
    media->videoSink()->videoFrame().toImage().save("/tmp/boarder-movie-decoded-frame.png");
    if(app.arguments().contains("--review"))pump(30000);
    fprintf(stdout,"MoviePlayerSmoke: PASS (encoding, native decoding, duration, seek, actions, clipboard)\n");
    return 0;
}
