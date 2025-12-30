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
    double maxWidth=4; // Max width in pixels -- TODO rename thickness
    double minWidth=1; // Min width in pixels
    VariableWidthMode variableWidthMode=Uniform;
    int stepCount=20; // Bézier sampling steps
    QColor foregroundColor = Qt::black;
    QColor backgroundColor = Qt::gray;
    ColorMode colorMode = SolidForeground;


    // taperControl > 1 → longer tapered ends, shorter full-width section
    // taperControl < 1 → shorter tapered ends, longer full-width section
    float taperControl = 0.5; // e.g. 1.0 = normal, 2.0 = more tapered ends
};

#endif
