/*
ColorPaletteWidget.cpp
A self-contained Qt widget implementing a ToonBoom-inspired color palette.
- HSV gradient bars (click/drag to choose H,S,V values)
- Opacity slider with numeric input
- Swatches grid (groups) with add/delete/save/load presets (JSON)
- Emits colorChanged(QColor) when user selects or edits a swatch or bars

Usage:
- Add this file to your Qt project and include "ColorPaletteWidget.h" where needed.
- Example: ColorPaletteWidget *w = new ColorPaletteWidget; w->show();

Notes:
- This is a single-file example combining header + implementation for convenience.
- The UI is constructed programmatically and is styled to be clean and modern.
*/

#ifndef ColorPaletteWidget_h
#define ColorPaletteWidget_h

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QSlider>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

// ----------------------- GradientBar -----------------------
class GradientBar : public QWidget {
    Q_OBJECT
public:
    enum Channel { Hue, Saturation, Value };
    GradientBar(Channel ch, QWidget* parent = nullptr);

    double value() const;
    void setValue(double v);
    Channel channel() const;

signals:
    void valueChanged(double v);

protected:
    void paintEvent(QPaintEvent *) override ;

    void mousePressEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;

private:
    void handleMouse(const QPoint &pos);

public:
    // Public setters for dependent channels
    void setHue(double h);
    void setSaturation(double s);
    void setValueForRender(double v);

private:
    Channel m_channel;
    double m_value;
    double m_hue {0.0};
    double m_saturation {1.0};
};

// ----------------------- ColorSwatchGrid -----------------------
class ColorSwatchGrid : public QWidget {
    Q_OBJECT
public:
    struct PaletteGroup { QString name; QVector<QColor> colors; };

    ColorSwatchGrid(QWidget* parent = nullptr);

    void setPalettes(const QVector<PaletteGroup>& p);
    QVector<PaletteGroup> palettes() const;

    void addColorToGroup(int groupIndex, const QColor &c);
    void addGroup(const QString &name);
    void removeColor(int groupIndex, int colorIndex);

signals:
    void swatchSelected(const QColor &c, int groupIndex, int colorIndex);

private:
    void rebuild();

    QVBoxLayout *m_layout{nullptr};
    QScrollArea *m_scroll{nullptr};
    QWidget *m_container{nullptr};
    QVBoxLayout *m_gridLayout{nullptr};
    QVector<PaletteGroup> m_groups;
};

// ----------------------- ColorPaletteWidget -----------------------
class ColorPaletteWidget : public QWidget {
    Q_OBJECT
public:
    ColorPaletteWidget(QWidget* parent = nullptr);

    QColor currentColor() const;

signals:
    void colorPicked(const QColor &c);
    void colorChanged(const QColor &c);

public slots:
    void setColor(const QColor &c);

    void savePalettesToFile();

    void loadPalettesFromFile();

private:
    void updateFromHSV();
    void updatePreview();

    GradientBar *m_hBar;
    GradientBar *m_sBar;
    GradientBar *m_vBar;
    QSlider *m_opacitySlider;
    QSpinBox *m_opacitySpin;
    QLabel *m_preview;
    QLineEdit *m_hexEdit;
    ColorSwatchGrid *m_swatches;

    double m_hue{0.0};
    double m_saturation{1.0};
    double m_value{1.0};
    int m_opacity{100};
};

// ----------------------- Demo main -----------------------
#ifdef COLOR_PALETTE_DEMO
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    ColorPaletteWidget w;
    w.show();
    return app.exec();
}
#endif

//#include "ColorPaletteWidget.moc"

#endif
