#ifndef PLAYBACK_TIMING_H
#define PLAYBACK_TIMING_H
#include <QString>
#include <QStringList>
#include <QtMath>

namespace PlaybackTiming {
// Counts distinct timeline frames actually painted, not timer ticks or exports.
class FrameRateCounter {
public:
    void reset() { *this = FrameRateCounter(); }
    void record(quint64 frameId, qint64 now) {
        if (m_lastPaint >= 0 && frameId == m_lastFrame) return;
        m_lastFrame = frameId;
        m_lastPaint = now;
        if (m_windowStart < 0) { m_windowStart = now; return; }
        ++m_frames;
        const qint64 elapsed = now - m_windowStart;
        if (elapsed >= 500) {
            m_rate = m_frames * 1000.0 / elapsed;
            m_windowStart = now;
            m_frames = 0;
        }
    }
    double fps(qint64 now) const {
        return m_lastPaint < 0 || now - m_lastPaint >= 1000 ? 0.0 : m_rate;
    }
private:
    qint64 m_windowStart = -1, m_lastPaint = -1;
    quint64 m_lastFrame = 0;
    int m_frames = 0;
    double m_rate = 0;
};

inline double validFps(double fps) { return qIsFinite(fps) && fps > 0 ? fps : 25.0; }
inline qint64 frameTime(double milliseconds, double fps) {
    fps = validFps(fps);
    return qRound64(qFloor(qMax(0.0, milliseconds) * fps / 1000.0) * 1000.0 / fps);
}
// Re-align each wakeup to the next sequence frame instead of accumulating a
// rounded timer interval (e.g. repeatedly using 33 ms for a 30 fps project).
inline int nextFrameDelay(double milliseconds, double fps) {
    fps = validFps(fps);
    const double next = (qFloor(milliseconds * fps / 1000.0) + 1) * 1000.0 / fps;
    return qMax(1, qCeil(next - milliseconds));
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
