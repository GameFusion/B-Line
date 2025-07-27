#ifndef StrokeProperties_h
#define StrokeProperties_h

#include <QColor>

struct StrokeProperties {

    enum VariableWidthMode {
        Uniform = 0,
        TaperIn,
        TaperOut,
        Pressure
    };

    enum ColorMode {
        SolidForeground = 0,
        SolidBackground,
        GradientBGtoFG,
        GradientFGtoBG
    };

    float smoothness=20; // 0.0 to 1.0
    double maxWidth=4; // Max width in pixels
    double minWidth=1; // Min width in pixels
    VariableWidthMode variableWidthMode=Uniform;
    int stepCount=20; // Bézier sampling steps
    QColor foregroundColor = Qt::black;
    QColor backgroundColor = Qt::gray;
    ColorMode colorMode = SolidForeground;
};

#endif
