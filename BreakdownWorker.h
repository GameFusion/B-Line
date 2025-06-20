#ifndef BreakdownWorker_h
#define BreakdownWorker_h

#include <QObject>
#include "ScriptBreakdown.h"

class BreakdownWorker : public QObject {
    Q_OBJECT
public:
    BreakdownWorker(GameFusion::ScriptBreakdown* breakdown)
        : breakdown(breakdown) {}

signals:
    void finished(bool success);
    //void shotCreated(const QString& sequence, const QString& scene, const QString& shotName);

public slots:
    void process() {
        bool ok = breakdown->breakdownScript(
            GameFusion::ScriptBreakdown::BreakdownMode::FullContext,
            false // sequences enabled
            );

        // Optional: emit per-shot creation here (if breakdownScript provides such callback)
        emit finished(ok);
    }

private:
    GameFusion::ScriptBreakdown* breakdown;
};

#endif
