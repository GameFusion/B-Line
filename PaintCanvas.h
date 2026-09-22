#ifndef PaintCanvas_h
#define PaintCanvas_h

#include <QGraphicsView>
#include <QPicture>
#include <QPointer>
#include "paintarea.h"

class QToolBar;
class QActionGroup;
class QLabel;

// An independent view of the integrated canvas's document and editing tools.
// Coordinates in this scene are logical canvas pixels, regardless of either zoom.
class PaintCanvas : public QGraphicsView
{
    Q_OBJECT
public:
    explicit PaintCanvas(QWidget *parent = nullptr);
    void setPaintArea(PaintArea *area);
    PaintArea *paintArea() const { return m_area; }
    void setHistoryActions(QAction *undo, QAction *redo);
    double zoomFactor() const { return transform().m11(); }
    void applyZoom(double zoom);
    void fitToBase();
    QPointF sourcePosition(const QPointF &viewportPosition) const;

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    bool viewportEvent(QEvent *event) override;

private:
    QAction *addIconAction(const QString &label, ushort glyph);
    void updateLightTableAction();
    void updatePlaybackView();
    void forwardMouse(QMouseEvent *event);
    void invalidateDrawing();
    void updateToolActions();
    void updateZoomLabel();
    void zoomAt(double zoom, const QPointF &position);
    QPointer<PaintArea> m_area;
    QToolBar *m_toolbar;
    QActionGroup *m_tools;
    QLabel *m_zoomLabel;
    QAction *m_lightTable;
    QPicture m_drawing;
    QImage m_strokeBackground;
    QRectF m_strokeBackgroundRect;
    double m_sourceZoom = 1.0;
    bool m_dirty = true;
    bool m_fitted = false;
    bool m_spaceDown = false;
    bool m_panning = false;
    bool m_editing = false;
    bool m_showPip = true;
    bool m_capturingPlaybackSnapshot = false;
    QPixmap m_frozenPlaybackView;
    QPoint m_panPosition;
};
#endif
