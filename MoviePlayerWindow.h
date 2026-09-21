#ifndef MOVIE_PLAYER_WINDOW_H
#define MOVIE_PLAYER_WINDOW_H
#include <QWidget>
class QMediaPlayer;
class MoviePlayerWindow : public QWidget {
public:
    explicit MoviePlayerWindow(const QString &filePath, QWidget *parent = nullptr);
    static QString encoderPath();
    static bool encode(const QString &directory, const QString &audioPath, const QString &outputPath,
                       double fps, double durationSeconds, QWidget *parent, QString *error);
private:
    QMediaPlayer *m_player;
};
#endif
