#include "StrokeAttributeDockWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QPushButton>
#include <QColorDialog>
#include <QJsonDocument>

#include "paintarea.h"
#include "ScriptBreakdown.h"

StrokeAttributeDockWidget::StrokeAttributeDockWidget(QWidget *parent)
    : QDockWidget("Stroke Attributes", parent)
{
    // Create the main widget and layout
    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setSpacing(4);
    layout->setContentsMargins(8, 8, 8, 8);

    previewArea = new PaintArea(this);
    previewArea->setFixedHeight(100);
    previewArea->setFpsDisplay(false);
    previewArea->setPipDisplay(false);
    layout->addWidget(previewArea);

    setupPreviewCurve();

    // Smoothness slider (0.0 to 1.0, mapped to 0-100)
    QHBoxLayout *titleValue = new QHBoxLayout;
    smoothnessSlider = new QSlider(Qt::Horizontal);
    smoothnessSlider->setRange(0, 100);
    smoothnessSlider->setValue(50); // Default: 0.5
    smoothnessLabel = new QLabel("0.50");
    titleValue->addWidget(new QLabel("Smoothness:"));
    titleValue->addWidget(smoothnessLabel);
    titleValue->addStretch(1);
    layout->addLayout(titleValue);
    layout->addWidget(smoothnessSlider);

    // Max width slider (0.1 to 20.0 pixels, scaled by 10)
    titleValue = new QHBoxLayout;
    maxWidthSlider = new QSlider(Qt::Horizontal);
    maxWidthSlider->setRange(1, 200);
    maxWidthSlider->setValue(20); // Default: 2.0 pixels
    maxWidthLabel = new QLabel("2.0 px");
    titleValue->addWidget(new QLabel("Max Width:"));
    titleValue->addWidget(maxWidthLabel);
    titleValue->addStretch(1);
    layout->addLayout(titleValue);
    layout->addWidget(maxWidthSlider);

    // Min width slider (0.1 to 20.0 pixels, scaled by 10)
    titleValue = new QHBoxLayout;
    minWidthSlider = new QSlider(Qt::Horizontal);
    minWidthSlider->setRange(1, 200);
    minWidthSlider->setValue(5); // Default: 0.5 pixels
    minWidthLabel = new QLabel("0.5 px");
    titleValue->addWidget(new QLabel("Min Width:"));
    titleValue->addWidget(minWidthLabel);
    titleValue->addStretch(1);
    layout->addLayout(titleValue);
    layout->addWidget(minWidthSlider);

    // Bézier sampling slider (5 to 100)
    titleValue = new QHBoxLayout;
    samplingSlider = new QSlider(Qt::Horizontal);
    samplingSlider->setRange(5, 100);
    samplingSlider->setValue(20); // Default: 20
    samplingLabel = new QLabel("20");
    titleValue->addWidget(new QLabel("Bézier Sampling:"));
    titleValue->addWidget(samplingLabel);
    titleValue->addStretch(1);
    layout->addLayout(titleValue);
    layout->addWidget(samplingSlider);

    // Bézier sampling slider (5 to 100)
    titleValue = new QHBoxLayout;
    taperSlider = new QSlider(Qt::Horizontal);
    taperSlider->setRange(0, 40);
    taperSlider->setValue(10); // Default: 10
    taperLabel = new QLabel("1");
    titleValue->addWidget(new QLabel("Taper timing:"));
    titleValue->addWidget(taperLabel);
    titleValue->addStretch(1);
    layout->addLayout(titleValue);
    layout->addWidget(taperSlider);

    // Variable width mode combo box
    layout->addWidget(new QLabel("Width Variation:"));
    variableWidthCombo = new QComboBox;
    variableWidthCombo->addItems({"Uniform", "Taper In", "Taper Out", "Pressure"});
    variableWidthCombo->setCurrentIndex(0); // Default: Uniform
    layout->addWidget(variableWidthCombo);

    // Color swatches
    QHBoxLayout *colorLayout = new QHBoxLayout;
    foregroundColorButton = new QPushButton;
    foregroundColorButton->setFixedSize(30, 30);
    foregroundColor = Qt::black;
    updateColorButtonStyle(foregroundColorButton, foregroundColor);
    backgroundColorButton = new QPushButton;
    backgroundColorButton->setFixedSize(30, 30);
    backgroundColor = Qt::white;
    updateColorButtonStyle(backgroundColorButton, backgroundColor);
    colorLayout->addWidget(new QLabel("Foreground:"));
    colorLayout->addWidget(foregroundColorButton);
    colorLayout->addWidget(new QLabel("Background:"));
    colorLayout->addWidget(backgroundColorButton);
    colorLayout->addStretch(1);
    layout->addLayout(colorLayout);

    // Color mode combo box
    layout->addWidget(new QLabel("Color Mode:"));
    colorModeCombo = new QComboBox;
    colorModeCombo->addItems({"Solid Foreground", "Solid Background", "Gradient BG to FG", "Gradient FG to BG"});
    colorModeCombo->setCurrentIndex(0); // Default: Solid Foreground
    layout->addWidget(colorModeCombo);

    // Add stretch to keep widgets compact
    layout->addStretch();

    // Set the widget for the dock
    setWidget(widget);

    // Apply stylesheet for compact and modern look
    widget->setStyleSheet(
        "QLabel { font-size: 12px; }"
        "QSlider, QComboBox, QPushButton { margin-bottom: 4px; }"
        "QPushButton { border: 1px solid #555; border-radius: 3px; }"
        );

    // Connect signals
    connect(smoothnessSlider, &QSlider::valueChanged, this, &StrokeAttributeDockWidget::updateSmoothnessLabel);
    connect(smoothnessSlider, &QSlider::valueChanged, this, &StrokeAttributeDockWidget::emitStrokeProperties);
    connect(maxWidthSlider, &QSlider::valueChanged, this, &StrokeAttributeDockWidget::updateMaxWidthLabel);
    //connect(maxWidthSlider, &QSlider::sliderReleased, this, &StrokeAttributeDockWidget::onMaxWidthReleased);
    connect(maxWidthSlider, &QSlider::valueChanged, this, &StrokeAttributeDockWidget::emitStrokeProperties);
    connect(minWidthSlider, &QSlider::valueChanged, this, &StrokeAttributeDockWidget::updateMinWidthLabel);
    connect(minWidthSlider, &QSlider::valueChanged, this, &StrokeAttributeDockWidget::emitStrokeProperties);
    connect(samplingSlider, &QSlider::valueChanged, this, &StrokeAttributeDockWidget::updateSamplingLabel);
    connect(samplingSlider, &QSlider::valueChanged, this, &StrokeAttributeDockWidget::emitStrokeProperties);
    connect(taperSlider, &QSlider::valueChanged, this, &StrokeAttributeDockWidget::updateTaperLabel);
    connect(taperSlider, &QSlider::valueChanged, this, &StrokeAttributeDockWidget::emitStrokeProperties);
    connect(variableWidthCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StrokeAttributeDockWidget::emitStrokeProperties);
    connect(foregroundColorButton, &QPushButton::clicked, this, &StrokeAttributeDockWidget::selectForegroundColor);
    connect(backgroundColorButton, &QPushButton::clicked, this, &StrokeAttributeDockWidget::selectBackgroundColor);
    connect(colorModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StrokeAttributeDockWidget::emitStrokeProperties);
}

void StrokeAttributeDockWidget::updateSmoothnessLabel(int value)
{
    smoothnessLabel->setText(QString("%1").arg(value / 100.0, 0, 'f', 2));
    emitStrokeProperties();
}

void StrokeAttributeDockWidget::updateMaxWidthLabel(int value)
{
    maxWidthLabel->setText(QString("%1 px").arg(value / 10.0, 0, 'f', 1));
    emitStrokeProperties();
}

void StrokeAttributeDockWidget::onMaxWidthReleased(int value)
{
    emitStrokeProperties();
}

void StrokeAttributeDockWidget::updateMinWidthLabel(int value)
{
    minWidthLabel->setText(QString("%1 px").arg(value / 10.0, 0, 'f', 1));
    emitStrokeProperties();
}

void StrokeAttributeDockWidget::updateSamplingLabel(int value)
{
    samplingLabel->setText(QString("%1").arg(value));
    emitStrokeProperties();
}

void StrokeAttributeDockWidget::updateTaperLabel(int value)
{
    double displayValue = value / 10.0; // convert int slider value to float
    taperLabel->setText(QString::number(displayValue, 'f', 1));
    emitStrokeProperties();
}

void StrokeAttributeDockWidget::selectForegroundColor()
{
    QColor color = QColorDialog::getColor(foregroundColor, this, "Select Foreground Color");
    if (color.isValid()) {
        foregroundColor = color;
        updateColorButtonStyle(foregroundColorButton, foregroundColor);
        emitStrokeProperties();
    }
}

void StrokeAttributeDockWidget::selectBackgroundColor()
{
    QColor color = QColorDialog::getColor(backgroundColor, this, "Select Background Color");
    if (color.isValid()) {
        backgroundColor = color;
        updateColorButtonStyle(backgroundColorButton, backgroundColor);
        emitStrokeProperties();
    }
}

void StrokeAttributeDockWidget::updateColorButtonStyle(QPushButton *button, const QColor &color)
{
    button->setStyleSheet(QString("QPushButton { background-color: %1; border: 1px solid #555; border-radius: 3px; }").arg(color.name()));
}

void StrokeAttributeDockWidget::emitStrokeProperties()
{

    if (skipUpdate) {
        return; // Premature return if skipUpdate is true
    }

    StrokeProperties props = getStrokeProperties();

    GameFusion::BezierCurve &curve = panel.layers.back().strokes.back();
    curve.setStrokeProperties(props);
    previewArea->setPanel(panel);
    previewArea->setStrokeProperties(props);

    emit strokePropertiesChanged(props);
}

StrokeProperties StrokeAttributeDockWidget::getStrokeProperties() const
{
    StrokeProperties props;

    props.smoothness = smoothnessSlider->value() / 100.0;
    props.maxWidth = maxWidthSlider->value() / 10.0;
    props.minWidth = minWidthSlider->value() / 10.0;
    props.variableWidthMode = static_cast<StrokeProperties::VariableWidthMode>(variableWidthCombo->currentIndex());
    props.stepCount = samplingSlider->value();
    props.foregroundColor = foregroundColor;
    props.backgroundColor = backgroundColor;
    props.colorMode = static_cast<StrokeProperties::ColorMode>(colorModeCombo->currentIndex());
    props.taperControl = taperSlider->value() / 10.0;

    return props;
}

// In StrokeProperties.cpp or relevant file
void StrokeAttributeDockWidget::setupPreviewCurve()
{
    // Inline JSON as a QString (multi-line string)
    QString jsonString = R"({
        "handles": [
            {
                "leftControl": {"x": 0, "y": 0, "z": 0},
                "point": {"x": 20, "y": 75, "z": 0},
                "rightControl": {"x": 35, "y": -45, "z": 0}
            },
            {
                "leftControl": {"x": -20, "y": 0, "z": 0},
                "point": {"x": 85, "y": 25, "z": 0},
                "rightControl": {"x": 20, "y": 0, "z": 0}
            },
            {
                "leftControl": {"x": -20, "y": 0, "z": 0},
                "point": {"x": 150, "y": 70, "z": 0},
                "rightControl": {"x": 20, "y": 0, "z": 0}
            },
            {
                "leftControl": {"x": -20, "y": 0, "z": 0},
                "point": {"x": 200, "y": 37, "z": 0},
                "rightControl": {"x": 20, "y": 0, "z": 0}
            },
            {
                "leftControl": {"x": -40, "y": 0, "z": 0},
                "point": {"x": 275, "y": 75, "z": 0},
                "rightControl": {"x": 0, "y": 0, "z": 0}
            }
        ],
        "strokeProperties": {
            "backgroundColor": {"a": 255, "b": 164, "g": 160, "r": 160},
            "colorMode": 0,
            "foregroundColor": {"a": 255, "b": 0, "g": 0, "r": 0},
            "maxWidth": 4,
            "minWidth": 1,
            "smoothness": 20,
            "stepCount": 20,
            "variableWidthMode": 0
        }
    })";

    // Parse JSON string to QJsonObject
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
    if (doc.isNull()) {
        qWarning() << "Failed to parse JSON";
        return;
    }
    QJsonObject inlineJson = doc.object();

    // Initialize BezierCurve from JSON
    GameFusion::BezierCurve stroke;
    stroke.fromJson(inlineJson);

    // Set up Layer and Panel
    GameFusion::Layer layer;
    layer.strokes.push_back(stroke);
    panel.layers.push_back(layer);

    // Set the panel in the preview area
    if (previewArea) {
        previewArea->setPanel(panel);
        previewArea->setToolMode(PaintArea::ToolMode::Edit);
    } else {
        qWarning() << "previewArea is null";
    }
}

void StrokeAttributeDockWidget::setStrokeProperties(const StrokeProperties &props) {
    GameFusion::BezierCurve &curve = panel.layers.back().strokes.back();
    curve.setStrokeProperties(props);
    previewArea->setPanel(panel);
    previewArea->setStrokeProperties(props);

    // Set skipUpdate to true to avoid feedback loop
    skipUpdate = true;

    // Update UI properties
    smoothnessSlider->setValue(static_cast<int>(props.smoothness * 100));
    maxWidthSlider->setValue(static_cast<int>(props.maxWidth * 10));
    minWidthSlider->setValue(static_cast<int>(props.minWidth * 10));
    samplingSlider->setValue(props.stepCount);
    taperSlider->setValue(static_cast<int>(props.taperControl * 10));
    variableWidthCombo->setCurrentIndex(static_cast<int>(props.variableWidthMode));
    colorModeCombo->setCurrentIndex(static_cast<int>(props.colorMode));
    foregroundColor = props.foregroundColor;
    updateColorButtonStyle(foregroundColorButton, foregroundColor);
    backgroundColor = props.backgroundColor;
    updateColorButtonStyle(backgroundColorButton, backgroundColor);

    // Reset skipUpdate to false after updates
    skipUpdate = false;
}
