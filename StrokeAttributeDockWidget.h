#ifndef STROKEATTRIBUTEDOCKWIDGET_H
#define STROKEATTRIBUTEDOCKWIDGET_H

#include <QDockWidget>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>

#include "ScriptBreakdown.h"
#include "StrokeProperties.h"

class PaintArea;

class StrokeAttributeDockWidget : public QDockWidget
{
    Q_OBJECT
public:


    explicit StrokeAttributeDockWidget(QWidget *parent = nullptr);

    StrokeProperties getStrokeProperties() const;

signals:
    void strokePropertiesChanged(const StrokeProperties &properties);

private slots:
    void updateSmoothnessLabel(int value);
    void updateMaxWidthLabel(int value);
    void updateMinWidthLabel(int value);
    void updateSamplingLabel(int value);
    void updateTaperLabel(int value);
    void emitStrokeProperties();
    void selectForegroundColor();
    void selectBackgroundColor();

private:
    void updateColorButtonStyle(QPushButton *button, const QColor &color);
    void setupPreviewCurve();

    QSlider *smoothnessSlider;
    QLabel *smoothnessLabel;
    QSlider *maxWidthSlider;
    QLabel *maxWidthLabel;
    QSlider *minWidthSlider;
    QLabel *minWidthLabel;
    QSlider *samplingSlider;
    QLabel *taperLabel;
    QSlider *taperSlider;
    QLabel *samplingLabel;
    QComboBox *variableWidthCombo;
    QPushButton *foregroundColorButton;
    QPushButton *backgroundColorButton;
    QComboBox *colorModeCombo;
    QColor foregroundColor;
    QColor backgroundColor;

    PaintArea *previewArea;

    GameFusion::Panel panel; // preview panel with curve
};

#endif // STROKEATTRIBUTEDOCKWIDGET_H
