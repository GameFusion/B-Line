#ifndef PLAYBACK_TIMING_H
#define PLAYBACK_TIMING_H
#include <QString>
#include <QStringList>
#include <QtMath>

namespace PlaybackTiming {
inline double validFps(double fps) { return qIsFinite(fps) && fps > 0 ? fps : 25.0; }
inline qint64 frameTime(double milliseconds, double fps) {
    fps = validFps(fps);
    return qRound64(qFloor(qMax(0.0, milliseconds) * fps / 1000.0) * 1000.0 / fps);
}
inline qint64 lastFrame(qint64 end, double fps) {
    fps = validFps(fps);
    return qRound64(qMax<qint64>(0, qCeil(end * fps / 1000.0) - 1) * 1000.0 / fps);
}
// Project frame rates are whole numbers; this is non-drop-frame sequence TC.
inline QString timecode(qint64 milliseconds, double fps, const QString &start = {}) {
    const int rate = qMax(1, qRound(validFps(fps)));
    qint64 frames = qRound64(qMax<qint64>(0, milliseconds) * validFps(fps) / 1000.0);
    const auto fields = start.split(':');
    if (fields.size() == 4) {
        bool ok[4]; int n[4];
        for (int i = 0; i < 4; ++i) n[i] = fields[i].toInt(&ok[i]);
        if (ok[0] && ok[1] && ok[2] && ok[3] && n[0] >= 0 &&
            n[1] >= 0 && n[1] < 60 && n[2] >= 0 && n[2] < 60 && n[3] >= 0 && n[3] < rate)
            frames += ((qint64(n[0]) * 60 + n[1]) * 60 + n[2]) * rate + n[3];
    }
    return QString("%1:%2:%3:%4")
        .arg(frames / (rate * 3600), 2, 10, QChar('0'))
        .arg((frames / (rate * 60)) % 60, 2, 10, QChar('0'))
        .arg((frames / rate) % 60, 2, 10, QChar('0'))
        .arg(frames % rate, 2, 10, QChar('0'));
}
}
#endif
