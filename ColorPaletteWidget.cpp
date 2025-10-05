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

#include "ColorPaletteWidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QPushButton>
#include <QGroupBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

// ----------------------- GradientBar -----------------------

    GradientBar::GradientBar(Channel ch, QWidget* parent) : QWidget(parent), m_channel(ch), m_value(0.0) {
        setMinimumHeight(20);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    double GradientBar::value() const { return m_value; }
    void GradientBar::setValue(double v) { m_value = qBound(0.0, v, 1.0); update(); }
    GradientBar::Channel GradientBar::channel() const { return m_channel; }

    void GradientBar::paintEvent(QPaintEvent *)  {
        QPainter p(this);
        QRect r = rect().adjusted(2,2,-2,-2);
        QLinearGradient g(r.topLeft(), r.topRight());
        if (m_channel == Hue) {
            // Hue gradient across 0..360
            for (int i=0;i<=6;i++) {
                double t = double(i)/6.0;
                QColor c; c.setHsvF(t, 1.0, 1.0);
                g.setColorAt(t, c);
            }
        } else if (m_channel == Saturation) {
            QColor c1, c2;
            c1.setHsvF( m_hue, 0.0, m_value );
            c2.setHsvF( m_hue, 1.0, m_value );
            g.setColorAt(0.0, c1);
            g.setColorAt(1.0, c2);
        } else { // Value
            QColor c1, c2;
            c1.setHsvF( m_hue, m_saturation, 0.0 );
            c2.setHsvF( m_hue, m_saturation, 1.0 );
            g.setColorAt(0.0, c1);
            g.setColorAt(1.0, c2);
        }
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(r, g);

        // draw handle
        int x = r.left() + int(m_value * r.width());
        QPen pen(Qt::black, 2);
        p.setPen(pen);
        p.drawEllipse(QPoint(x, r.center().y()), 6, 6);
    }

    void GradientBar::mousePressEvent(QMouseEvent *ev) { handleMouse(ev->pos()); }
    void GradientBar::mouseMoveEvent(QMouseEvent *ev) { if (ev->buttons() & Qt::LeftButton) handleMouse(ev->pos()); }


    void GradientBar::handleMouse(const QPoint &pos) {
        QRect r = rect().adjusted(2,2,-2,-2);
        double v = double(pos.x() - r.left()) / double(qMax(1, r.width()));
        v = qBound(0.0, v, 1.0);
        if (!qFuzzyCompare(v, m_value)) {
            m_value = v;
            emit valueChanged(m_value);
            update();
        }
    }


    // Public setters for dependent channels
    void GradientBar::setHue(double h) { m_hue = h; if (m_channel != Hue) update(); }
    void GradientBar::setSaturation(double s) { m_saturation = s; if (m_channel == Value) update(); }
    void GradientBar::setValueForRender(double v) { m_value = v; if (m_channel != Value) update(); }


// ----------------------- ColorSwatchGrid -----------------------

    ColorSwatchGrid::ColorSwatchGrid(QWidget* parent) : QWidget(parent) {
        m_layout = new QVBoxLayout(this);
        m_layout->setContentsMargins(0,0,0,0);
        m_scroll = new QScrollArea(this);
        m_scroll->setWidgetResizable(true);
        m_container = new QWidget;
        m_gridLayout = new QVBoxLayout(m_container);
        m_gridLayout->setSpacing(8);
        m_scroll->setWidget(m_container);
        m_layout->addWidget(m_scroll);
    }

    void ColorSwatchGrid::setPalettes(const QVector<PaletteGroup>& p) { m_groups = p; rebuild(); }
    QVector<ColorSwatchGrid::PaletteGroup> ColorSwatchGrid::palettes() const { return m_groups; }

    void ColorSwatchGrid::addColorToGroup(int groupIndex, const QColor &c) {
        if (groupIndex >=0 && groupIndex < m_groups.size()) {
            m_groups[groupIndex].colors.append(c);
            rebuild();
        }
    }
    void ColorSwatchGrid::addGroup(const QString &name) { m_groups.append({name, {}}); rebuild(); }
    void ColorSwatchGrid::removeColor(int groupIndex, int colorIndex) {
        if (groupIndex>=0 && groupIndex<m_groups.size()) {
            auto &g = m_groups[groupIndex].colors;
            if (colorIndex>=0 && colorIndex<g.size()) g.remove(colorIndex);
            rebuild();
        }
    }




    void ColorSwatchGrid::rebuild() {
        QLayoutItem *child;
        while ((child = m_gridLayout->takeAt(0)) != nullptr) {
            delete child->widget(); delete child;
        }

        const int cols = 8;
        for (int gi=0; gi<m_groups.size(); ++gi) {
            const PaletteGroup &g = m_groups[gi];
            QLabel *lbl = new QLabel(g.name);
            lbl->setStyleSheet("font-weight:600;padding:4px;");
            m_gridLayout->addWidget(lbl);

            QWidget *gridWidget = new QWidget;
            QGridLayout *gl = new QGridLayout(gridWidget);
            gl->setSpacing(6);
            gl->setContentsMargins(2,2,2,2);

            for (int i=0;i<g.colors.size();++i) {
                QPushButton *b = new QPushButton;
                b->setFixedSize(28,20);
                QColor c = g.colors[i];
                b->setStyleSheet(QString("background-color: %1; border: 1px solid rgba(0,0,0,0.25); border-radius:3px;")
                                     .arg(c.name()));
                b->setProperty("groupIndex", gi);
                b->setProperty("colorIndex", i);
                connect(b, &QPushButton::clicked, this, [this,b](){
                    int gi = b->property("groupIndex").toInt();
                    int ci = b->property("colorIndex").toInt();
                    QColor c = m_groups[gi].colors[ci];
                    emit swatchSelected(c, gi, ci);
                });
                int row = i / cols;
                int col = i % cols;
                gl->addWidget(b, row, col);
            }
            m_gridLayout->addWidget(gridWidget);
        }
        m_container->adjustSize();
    }




// ----------------------- ColorPaletteWidget -----------------------

    ColorPaletteWidget::ColorPaletteWidget(QWidget* parent) : QWidget(parent) {
        //setMinimumSize(420, 360);
        QVBoxLayout *main = new QVBoxLayout(this);
        main->setSpacing(4);

        // Top: HSV bars + opacity
        //QGroupBox *controlsBox = new QGroupBox("Color Controls");
        //QVBoxLayout *cbLayout = new QVBoxLayout(controlsBox);

        QVBoxLayout *cbLayout = new QVBoxLayout();

        m_hBar = new GradientBar(GradientBar::Hue);
        m_sBar = new GradientBar(GradientBar::Saturation);
        m_vBar = new GradientBar(GradientBar::Value);

        cbLayout->addWidget(new QLabel("Hue")); cbLayout->addWidget(m_hBar);
        cbLayout->addWidget(new QLabel("Saturation")); cbLayout->addWidget(m_sBar);
        cbLayout->addWidget(new QLabel("Value")); cbLayout->addWidget(m_vBar);


        QHBoxLayout *opacityRow = new QHBoxLayout;
        opacityRow->addWidget(new QLabel("Opacity"));
        m_opacitySlider = new QSlider(Qt::Horizontal);
        m_opacitySlider->setRange(0, 100);
        m_opacitySlider->setValue(100);
        m_opacitySpin = new QSpinBox; m_opacitySpin->setRange(0,100); m_opacitySpin->setValue(100);
        connect(m_opacitySlider, &QSlider::valueChanged, m_opacitySpin, &QSpinBox::setValue);
        connect(m_opacitySpin, QOverload<int>::of(&QSpinBox::valueChanged), m_opacitySlider, &QSlider::setValue);
        opacityRow->addWidget(m_opacitySlider);
        opacityRow->addWidget(m_opacitySpin);
        cbLayout->addLayout(opacityRow);


        // Color preview and hex input
        QHBoxLayout *previewRow = new QHBoxLayout;
        m_preview = new QLabel;
        //m_preview->setFixedSize(44,44);
        m_preview->setStyleSheet("border-radius:6px;border:1px solid rgba(0,0,0,0.2);");
        m_hexEdit = new QLineEdit; m_hexEdit->setMaximumWidth(100);
        previewRow->addWidget(m_preview);
        previewRow->addWidget(new QLabel("Hex")); previewRow->addWidget(m_hexEdit);
        previewRow->addStretch();
        cbLayout->addLayout(previewRow);

        //main->addWidget(controlsBox);
        main->addLayout(cbLayout);

        // Middle: swatches grid and controls
        QGroupBox *swatchBox = new QGroupBox("Swatches");
        QVBoxLayout *sbLayout = new QVBoxLayout(swatchBox);

        m_swatches = new ColorSwatchGrid;
        sbLayout->addWidget(m_swatches);



        QHBoxLayout *swControls = new QHBoxLayout;
        QPushButton *addGroupBtn = new QPushButton("+ Grp"); // Add Group
        QPushButton *addColorBtn = new QPushButton("+ Clr"); // Add Color
        QPushButton *removeColorBtn = new QPushButton("- Clr"); // Remove Color
        QPushButton *saveBtn = new QPushButton("S"); // Save
        QPushButton *loadBtn = new QPushButton("L"); // Load

        swControls->addWidget(addGroupBtn);
        swControls->addWidget(addColorBtn);
        swControls->addWidget(removeColorBtn);
        swControls->addStretch();
        swControls->addWidget(saveBtn);
        swControls->addWidget(loadBtn);

        sbLayout->addLayout(swControls);

        main->addWidget(swatchBox, 1);

        //return;

        // Bottom: actions
        /*
        QHBoxLayout *actions = new QHBoxLayout;
        QPushButton *ok = new QPushButton("Select");
        QPushButton *cancel = new QPushButton("Close");
        actions->addStretch(); actions->addWidget(ok); actions->addWidget(cancel);
        main->addLayout(actions);
        connect(ok, &QPushButton::clicked, this, [this]{ emit colorPicked(currentColor()); });
        connect(cancel, &QPushButton::clicked, this, [this]{ this->close(); });
        */

        // default palette
        ColorSwatchGrid::PaletteGroup g1; g1.name = "Default";
        g1.colors = { Qt::black, Qt::white, QColor(200, 30, 30), QColor(30,200,30), QColor(30,30,200) };
        m_swatches->setPalettes({g1});

        // Wire up interactions
        connect(m_hBar, &GradientBar::valueChanged, this, [this](double v){ m_hue = v; updateFromHSV(); });
        connect(m_sBar, &GradientBar::valueChanged, this, [this](double v){ m_saturation = v; updateFromHSV(); });
        connect(m_vBar, &GradientBar::valueChanged, this, [this](double v){ m_value = v; updateFromHSV(); });
        connect(m_opacitySlider, &QSlider::valueChanged, this, [this](int v){ updatePreview(); emit colorChanged(currentColor()); });
        connect(m_hexEdit, &QLineEdit::editingFinished, this, [this]{ QColor c(m_hexEdit->text()); if (c.isValid()) setColor(c); });
        connect(m_swatches, &ColorSwatchGrid::swatchSelected, this, [this](const QColor &c,int gi,int ci){ setColor(c); emit colorPicked(c); });

        connect(addGroupBtn, &QPushButton::clicked, this, [this]{ bool ok; QString name = QInputDialog::getText(this, "Group name", "Group name:", QLineEdit::Normal, "New Group", &ok); if (ok && !name.isEmpty()) { m_swatches->addGroup(name); } });
        connect(addColorBtn, &QPushButton::clicked, this, [this]{ m_swatches->addColorToGroup(0, currentColor()); });
        connect(removeColorBtn, &QPushButton::clicked, this, [this]{ /* simple remove last in group 0 for demo */ m_swatches->removeColor(0, m_swatches->palettes()[0].colors.size()-1); });

        connect(saveBtn, &QPushButton::clicked, this, &ColorPaletteWidget::savePalettesToFile);
        connect(loadBtn, &QPushButton::clicked, this, &ColorPaletteWidget::loadPalettesFromFile);



        updateFromHSV();
    }

    QColor ColorPaletteWidget::currentColor() const {
        QColor c; c.setHsvF(m_hue, m_saturation, m_value, m_opacity/100.0);
        return c;
    }

    void ColorPaletteWidget::setColor(const QColor &c) {
        float h,s,v,a;
        c.getHsvF(&h, &s, &v, &a);
        m_hue = h; m_saturation = s; m_value = v; m_opacity = int(a*100);
        m_hBar->setValue(h);
        m_sBar->setHue(h); m_sBar->setValue(s);
        m_vBar->setHue(h); m_vBar->setSaturation(s); m_vBar->setValueForRender(v);
        m_opacitySlider->setValue(m_opacity);
        m_hexEdit->setText(c.name());
        updatePreview();
        emit colorChanged(currentColor());
    }

    void ColorPaletteWidget::savePalettesToFile() {
        QString fn = QFileDialog::getSaveFileName(this, "Save Palettes", QString(), "JSON (*.json)");
        if (fn.isEmpty()) return;
        QJsonArray groups;
        auto data = m_swatches->palettes();
        for (const auto &g : data) {
            QJsonObject go; go["name"] = g.name;
            QJsonArray arr;
            for (const QColor &c : g.colors) {
                QJsonObject co;
                co["r"] = c.red(); co["g"] = c.green(); co["b"] = c.blue(); co["a"] = c.alpha();
                arr.append(co);
            }
            go["colors"] = arr;
            groups.append(go);
        }
        QJsonDocument doc(groups);
        QFile f(fn);
        if (!f.open(QIODevice::WriteOnly)) return;
        f.write(doc.toJson());
        f.close();
    }

    void ColorPaletteWidget::loadPalettesFromFile() {
        QString fn = QFileDialog::getOpenFileName(this, "Load Palettes", QString(), "JSON (*.json)");
        if (fn.isEmpty()) return;
        QFile f(fn);
        if (!f.open(QIODevice::ReadOnly)) return;
        QByteArray b = f.readAll(); f.close();
        QJsonDocument doc = QJsonDocument::fromJson(b);
        if (!doc.isArray()) return;
        QVector<ColorSwatchGrid::PaletteGroup> groups;
        for (const QJsonValue &vv : doc.array()) {
            if (!vv.isObject()) continue;
            QJsonObject go = vv.toObject();
            ColorSwatchGrid::PaletteGroup g;
            g.name = go.value("name").toString("Group");
            QJsonArray carr = go.value("colors").toArray();
            for (const QJsonValue &cv : carr) {
                if (!cv.isObject()) continue;
                QJsonObject co = cv.toObject();
                QColor c(co.value("r").toInt(), co.value("g").toInt(), co.value("b").toInt(), co.value("a").toInt());
                g.colors.append(c);
            }
            groups.append(g);
        }
        m_swatches->setPalettes(groups);
    }


    void ColorPaletteWidget::updateFromHSV() {
        m_hBar->setValue(m_hue);
        m_sBar->setHue(m_hue); m_sBar->setValue(m_saturation);
        m_vBar->setHue(m_hue); m_vBar->setSaturation(m_saturation); m_vBar->setValueForRender(m_value);
        updatePreview();
        emit colorChanged(currentColor());
    }
    void ColorPaletteWidget::updatePreview() {
        QColor c = currentColor();
        QString st = QString("background-color: %1; border-radius:6px; border:1px solid rgba(0,0,0,0.2);").arg(c.name(QColor::HexArgb));
        m_preview->setStyleSheet(st);
        m_hexEdit->setText(c.name(QColor::HexRgb));
    }



// ----------------------- Demo main -----------------------
#ifdef COLOR_PALETTE_DEMO
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    ColorPaletteWidget w;
    w.show();
    return app.exec();
}
#endif

