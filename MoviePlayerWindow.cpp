#include "MoviePlayerWindow.h"
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QClipboard>
#include <QApplication>
#include <QFileInfo>
#include <QDir>
#include <QDesktopServices>
#include <QProcess>
#include <QStandardPaths>
#include <QProgressDialog>
#include <QEventLoop>
#include <QTimer>

namespace {
QString clockText(qint64 ms) {
    const qint64 seconds=qMax<qint64>(0,ms)/1000;
    return QString("%1:%2:%3").arg(seconds/3600,2,10,QChar('0')).arg(seconds/60%60,2,10,QChar('0')).arg(seconds%60,2,10,QChar('0'));
}
}
MoviePlayerWindow::MoviePlayerWindow(const QString &filePath,QWidget *parent)
    : QWidget(parent,Qt::Window),m_player(new QMediaPlayer(this)) {
    setAttribute(Qt::WA_DeleteOnClose);setObjectName("exportMoviePlayer");
    const QString absolute=QFileInfo(filePath).absoluteFilePath();
    setWindowTitle(tr("Movie · %1").arg(QFileInfo(absolute).fileName()));resize(960,640);setMinimumSize(480,320);
    auto *layout=new QVBoxLayout(this);auto *video=new QVideoWidget(this);video->setMinimumSize(160,90);layout->addWidget(video,1);
    auto *audio=new QAudioOutput(this);audio->setVolume(0.8);m_player->setAudioOutput(audio);m_player->setVideoOutput(video);
    auto *status=new QLabel(this);status->setObjectName("movieStatus");status->setWordWrap(true);layout->addWidget(status);
    auto *row=new QHBoxLayout;layout->addLayout(row);
    auto *play=new QPushButton(tr("Play"),this);play->setObjectName("moviePlayPause");row->addWidget(play);
    auto *seek=new QSlider(Qt::Horizontal,this);seek->setObjectName("movieSeek");seek->setRange(0,10000);seek->setEnabled(false);row->addWidget(seek,1);
    auto *time=new QLabel("00:00:00 / 00:00:00",this);row->addWidget(time);
    auto *volume=new QSlider(Qt::Horizontal,this);volume->setRange(0,100);volume->setValue(80);volume->setMaximumWidth(90);volume->setToolTip(tr("Volume"));volume->setAccessibleName(tr("Volume"));row->addWidget(volume);
    connect(volume,&QSlider::valueChanged,audio,[audio](int v){audio->setVolume(v/100.0);});
    connect(play,&QPushButton::clicked,this,[this]{if(m_player->isPlaying())m_player->pause();else {if(m_player->mediaStatus()==QMediaPlayer::EndOfMedia)m_player->setPosition(0);m_player->play();}});
    connect(m_player,&QMediaPlayer::playbackStateChanged,play,[play](QMediaPlayer::PlaybackState state){play->setText(state==QMediaPlayer::PlayingState?tr("Pause"):tr("Play"));});
    connect(seek,&QSlider::sliderMoved,m_player,[this](int value){m_player->setPosition(qRound64(m_player->duration()*(value/10000.0)));});
    connect(m_player,&QMediaPlayer::seekableChanged,seek,&QSlider::setEnabled);
    auto updateTime=[this,seek,time]{if(!seek->isSliderDown()&&m_player->duration()>0)seek->setValue(int(m_player->position()*10000/m_player->duration()));time->setText(clockText(m_player->position())+" / "+clockText(m_player->duration()));};
    connect(m_player,&QMediaPlayer::positionChanged,this,updateTime);connect(m_player,&QMediaPlayer::durationChanged,this,updateTime);
    connect(m_player,&QMediaPlayer::errorOccurred,status,[status](QMediaPlayer::Error,const QString &message){status->setText(tr("This movie could not be played: %1. You can still reveal or copy its path.").arg(message));});
    auto *footer=new QHBoxLayout;layout->addLayout(footer);auto *file=new QLabel(QFileInfo(absolute).fileName(),this);file->setToolTip(absolute);file->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);footer->addWidget(file,1);
#ifdef Q_OS_MAC
    const QString revealText=tr("Show in Finder");
#elif defined(Q_OS_WIN)
    const QString revealText=tr("Show in Explorer");
#else
    const QString revealText=tr("Show in Folder");
#endif
    auto *reveal=new QPushButton(revealText,this);reveal->setObjectName("movieReveal");footer->addWidget(reveal);
    connect(reveal,&QPushButton::clicked,this,[absolute,status]{
        bool ok=false;
#ifdef Q_OS_MAC
        ok=QProcess::startDetached("/usr/bin/open",{"-R",absolute});
#elif defined(Q_OS_WIN)
        ok=QProcess::startDetached("explorer.exe",{QString("/select,%1").arg(QDir::toNativeSeparators(absolute))});
#else
        ok=QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(absolute).absolutePath()));
#endif
        if(!ok)status->setText(tr("Could not reveal the movie. Use Copy Path to locate it."));
    });
    auto *copy=new QPushButton(tr("Copy Path"),this);copy->setObjectName("movieCopyPath");footer->addWidget(copy);
    connect(copy,&QPushButton::clicked,this,[absolute,copy]{QApplication::clipboard()->setText(QDir::toNativeSeparators(absolute));copy->setText(tr("Copied"));QTimer::singleShot(1800,copy,[copy]{copy->setText(tr("Copy Path"));});});
    m_player->setSource(QUrl::fromLocalFile(absolute));m_player->play();
}
QString MoviePlayerWindow::encoderPath() {
    QString path=QStandardPaths::findExecutable("ffmpeg");
    if(!path.isEmpty())return path;
    return QStandardPaths::findExecutable("ffmpeg",{QCoreApplication::applicationDirPath(),"/opt/homebrew/bin","/usr/local/bin",QDir::homePath()+"/homebrew/bin"});
}
bool MoviePlayerWindow::encode(const QString &directory,const QString &audioPath,const QString &outputPath,
                              double fps,double durationSeconds,QWidget *parent,QString *error) {
    const QString executable=encoderPath();
    if(executable.isEmpty()){*error=tr("FFmpeg is required to create an MP4 movie. Install it or add it to PATH.");return false;}
    QProcess encoder;
    QStringList args{"-hide_banner","-loglevel","error","-nostdin","-y","-framerate",QString::number(fps,'g',10),"-i",QDir(directory).filePath("frame_%05d.png")};
    if(!audioPath.isEmpty())args<<"-i"<<audioPath<<"-c:a"<<"aac"<<"-b:a"<<"192k";
    args<<"-c:v"<<"libx264"<<"-pix_fmt"<<"yuv420p"<<"-vf"<<"pad=ceil(iw/2)*2:ceil(ih/2)*2"<<"-movflags"<<"+faststart"<<"-t"<<QString::number(durationSeconds,'f',6)<<outputPath;
    QProgressDialog progress(tr("Creating movie…"),tr("Cancel"),0,0,parent);progress.setWindowModality(Qt::ApplicationModal);progress.setMinimumDuration(0);
    QEventLoop loop;bool cancelled=false;QByteArray stderrTail;
    QObject::connect(&encoder,&QProcess::readyReadStandardError,&loop,[&]{stderrTail+=encoder.readAllStandardError();stderrTail=stderrTail.right(8000);});
    QObject::connect(&encoder,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),&loop,&QEventLoop::quit);
    QObject::connect(&encoder,&QProcess::errorOccurred,&loop,[&](QProcess::ProcessError e){if(e==QProcess::FailedToStart)loop.quit();});
    QObject::connect(&progress,&QProgressDialog::canceled,&loop,[&]{cancelled=true;encoder.kill();});
    encoder.start(executable,args);loop.exec();
    progress.reset();
    if(cancelled||encoder.exitStatus()!=QProcess::NormalExit||encoder.exitCode()!=0||!QFileInfo(outputPath).isFile()||QFileInfo(outputPath).size()==0){
        *error=cancelled?tr("Movie export cancelled. Rendered frames have been kept."):tr("Could not create the movie: %1").arg(stderrTail.isEmpty()?encoder.errorString():QString::fromUtf8(stderrTail));return false;
    }
    return true;
}
