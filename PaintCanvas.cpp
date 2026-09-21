#include "PaintCanvas.h"

#include <QActionGroup>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTabletEvent>
#include <QToolBar>
#include <cmath>

PaintCanvas::PaintCanvas(QWidget *parent) : QGraphicsView(parent),
    m_toolbar(new QToolBar(this)), m_tools(new QActionGroup(this)),
    m_zoomLabel(new QLabel(this))
{
    setWindowFlag(Qt::Window);
    setWindowTitle(tr("B-Line — Drawing Workspace"));
    setObjectName("drawingWorkspace");
    resize(1200, 800);
    setScene(new QGraphicsScene(this));
    setSceneRect(-1000000, -1000000, 2000000, 2000000);
    // Use Qt's ordinary raster viewport. No OpenGL surface or overlay intercepts input.
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    setTransformationAnchor(NoAnchor);
    setResizeAnchor(AnchorViewCenter);
    setMouseTracking(true);
    viewport()->setTabletTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setFrameShape(QFrame::NoFrame);
    setBackgroundBrush(Qt::white);
    m_toolbar->setMovable(false);
    m_toolbar->setFloatable(false);
    m_toolbar->setIconSize(QSize(16, 16));
    m_tools->setExclusive(true);
    const QList<QPair<QString, PaintArea::ToolMode>> tools = {
        {tr("Draw"), PaintArea::ToolMode::Paint}, {tr("Text"), PaintArea::ToolMode::Text},
        {tr("Select"), PaintArea::ToolMode::Select}, {tr("Erase"), PaintArea::ToolMode::Erase},
        {tr("Edit"), PaintArea::ToolMode::Edit}, {tr("Camera"), PaintArea::ToolMode::Camera},
        {tr("Layer"), PaintArea::ToolMode::Layer}};
    for (const auto &tool : tools) {
        QAction *action = m_toolbar->addAction(tool.first);
        action->setCheckable(true);
        action->setData(int(tool.second));
        m_tools->addAction(action);
        connect(action, &QAction::triggered, this, [this, tool] {
            if (m_area && !m_editing) m_area->setToolMode(tool.second);
            updateToolActions();
        });
    }
    m_toolbar->addSeparator();
    m_toolbar->addAction(tr("−"), this, [this] { applyZoom(zoomFactor() / 1.25); });
    m_toolbar->addWidget(m_zoomLabel);
    m_toolbar->addAction(tr("+"), this, [this] { applyZoom(zoomFactor() * 1.25); });
    m_toolbar->addAction(tr("100%"), this, [this] { applyZoom(1.0); });
    m_toolbar->addAction(tr("Fit"), this, &PaintCanvas::fitToBase);
    m_toolbar->addSeparator();
    QAction *lightTable = m_toolbar->addAction(tr("Light table"));
    lightTable->setCheckable(true);
    connect(lightTable, &QAction::toggled, this, [this](bool on) { if (m_area) m_area->setLightTableMode(on); });
    QAction *pip = m_toolbar->addAction(tr("Camera preview"));
    pip->setCheckable(true);
    pip->setChecked(true);
    connect(pip, &QAction::toggled, this, [this](bool on) { m_showPip = on; viewport()->update(); });
    m_toolbar->setToolTip(tr("Wheel: zoom · Trackpad: pan · Space-drag or middle button: pan · F: fit"));
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, &PaintCanvas::invalidateDrawing);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &PaintCanvas::invalidateDrawing);
    updateZoomLabel();
}

void PaintCanvas::setPaintArea(PaintArea *area)
{
    if (m_area == area) return;
    if (m_area) {
        for (auto *action : m_area->actions()) removeAction(action);
        disconnect(m_area, nullptr, this, nullptr);
    }
    m_area = area;
    if (area) {
        for (auto *action : area->actions()) addAction(action);
        connect(area, &PaintArea::workspaceChanged, this, &PaintCanvas::invalidateDrawing);
        connect(area, &PaintArea::toolModeChanged, this, &PaintCanvas::updateToolActions);
        connect(area, &QObject::destroyed, this, [this] { m_drawing = QPicture(); invalidateDrawing(); });
    }
    m_drawing = QPicture();
    updateToolActions();
    invalidateDrawing();
    if (area && isVisible() && !m_fitted) fitToBase();
}

void PaintCanvas::setHistoryActions(QAction *undo, QAction *redo)
{
    m_toolbar->addSeparator();
    m_toolbar->addAction(undo);
    m_toolbar->addAction(redo);
    addAction(undo);
    addAction(redo);
}

void PaintCanvas::invalidateDrawing()
{
    m_dirty = true;
    viewport()->update(); // Qt coalesces updates; no timer and no polling.
}

void PaintCanvas::updateToolActions()
{
    for (auto *action : m_tools->actions()) {
        const QSignalBlocker block(action);
        action->setEnabled(m_area);
        action->setChecked(m_area && action->data().toInt() == int(m_area->toolMode()));
    }
}

void PaintCanvas::drawBackground(QPainter *painter, const QRectF &rect)
{
    painter->fillRect(rect, Qt::white);
    if (!m_area) return;
    if (m_dirty && !(m_area->interactionActive() && !m_area->workspaceInteractionActive())) {
        m_dirty = false;
        m_sourceZoom = m_area->zoomFactor();
        const QRectF visible = viewportTransform().inverted().mapRect(QRectF(viewport()->rect()));
        const QTransform source = QTransform::fromScale(m_sourceZoom, m_sourceZoom);
        m_drawing = m_area->workspacePicture(source.mapRect(visible),
            zoomFactor() * viewport()->devicePixelRatioF() / m_sourceZoom);
    }
    painter->save();
    painter->scale(1.0 / m_sourceZoom, 1.0 / m_sourceZoom);
    // QPicture uses a fixed logical DPI; compensate so model pixels stay pixels
    // on macOS, offscreen tests and HiDPI screens alike.
    painter->scale(qreal(m_drawing.logicalDpiX()) / painter->device()->logicalDpiX(),
                   qreal(m_drawing.logicalDpiY()) / painter->device()->logicalDpiY());
    painter->drawPicture(QPointF(), m_drawing);
    painter->restore();
}

void PaintCanvas::paintEvent(QPaintEvent *event)
{
    QGraphicsView::paintEvent(event);
    if (m_showPip && m_area && m_area->hasPipImage()) {
        QPainter painter(viewport());
        const QSize size = m_area->currentPipImage().size().scaled(QSize(240, 135), Qt::KeepAspectRatio);
        const QRect preview(QPoint(16, qMax(16, viewport()->height() - size.height() - 16)), size);
        painter.fillRect(preview.adjusted(-2, -2, 2, 2), QColor(40, 40, 40));
        painter.drawImage(preview, m_area->currentPipImage());
        m_area->drawPlaybackOverlay(painter, preview);
    } else if (m_area) {
        QPainter painter(viewport());
        m_area->drawPlaybackOverlay(painter, viewport()->rect());
    }
}

void PaintCanvas::resizeEvent(QResizeEvent *event)
{
    const int height = m_toolbar->sizeHint().height();
    setViewportMargins(0, height, 0, 0);
    m_toolbar->setGeometry(0, 0, width(), height);
    QGraphicsView::resizeEvent(event);
    invalidateDrawing();
}

void PaintCanvas::showEvent(QShowEvent *event)
{
    QGraphicsView::showEvent(event);
    if (m_area && !m_fitted) fitToBase();
    invalidateDrawing();
}

void PaintCanvas::updateZoomLabel()
{
    m_zoomLabel->setText(QString::number(zoomFactor() * 100.0, 'f', 0) + "%");
}

void PaintCanvas::zoomAt(double zoom, const QPointF &position)
{
    if (m_editing || !std::isfinite(zoom)) return;
    zoom = qBound(0.02, zoom, 32.0);
    const QPointF before = mapToScene(position.toPoint());
    setTransform(QTransform::fromScale(zoom, zoom));
    const QPointF after = mapToScene(position.toPoint());
    const QPointF center = mapToScene(viewport()->rect().center());
    centerOn(center + before - after);
    m_fitted = true;
    invalidateDrawing();
    updateZoomLabel();
}

void PaintCanvas::applyZoom(double zoom)
{
    zoomAt(zoom, viewport()->rect().center());
}

void PaintCanvas::fitToBase()
{
    if (!m_area || m_editing) return;
    const QSize canvas = m_area->canvasSizePx();
    const double zoom = qMin((viewport()->width() - 48.0) / canvas.width(),
                             (viewport()->height() - 48.0) / canvas.height());
    applyZoom(zoom);
    centerOn(QRectF(QPointF(), canvas).center());
}

QPointF PaintCanvas::sourcePosition(const QPointF &position) const
{
    return viewportTransform().inverted().map(position) * (m_area ? m_area->zoomFactor() : 1.0);
}

void PaintCanvas::forwardMouse(QMouseEvent *event)
{
    if (!m_area) return;
    const QPointF local = sourcePosition(event->position());
    QMouseEvent mapped(event->type(), local, local, event->globalPosition(),
                       event->button(), event->buttons(), event->modifiers(), event->pointingDevice());
    m_area->sendWorkspaceEvent(&mapped);
    event->accept();
}

void PaintCanvas::mousePressEvent(QMouseEvent *event)
{
    setFocus(Qt::MouseFocusReason);
    if (event->button() == Qt::MiddleButton || (m_spaceDown && event->button() == Qt::LeftButton)) {
        m_panning = true;
        m_panPosition = event->pos();
        viewport()->setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) m_editing = true;
    forwardMouse(event);
}

void PaintCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (m_panning) {
        const QPoint delta = event->pos() - m_panPosition;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        m_panPosition = event->pos();
        event->accept();
        return;
    }
    forwardMouse(event);
}

void PaintCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_panning && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        m_panning = false;
        viewport()->setCursor(m_spaceDown ? Qt::OpenHandCursor : Qt::ArrowCursor);
        event->accept();
        return;
    }
    forwardMouse(event);
    if (event->button() == Qt::LeftButton) m_editing = false;
}

void PaintCanvas::wheelEvent(QWheelEvent *event)
{
    if (m_editing) { event->accept(); return; }
    if (!event->pixelDelta().isNull() && !(event->modifiers() & (Qt::ControlModifier | Qt::MetaModifier))) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - event->pixelDelta().x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - event->pixelDelta().y());
    } else {
        zoomAt(zoomFactor() * std::pow(1.0015, event->angleDelta().y()), event->position());
    }
    event->accept();
}

void PaintCanvas::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !m_editing) {
        m_spaceDown = true; viewport()->setCursor(Qt::OpenHandCursor); event->accept(); return;
    }
    if (event->key() == Qt::Key_F && event->modifiers() == Qt::NoModifier) { fitToBase(); return; }
    if (m_area) m_area->sendWorkspaceEvent(event);
}

void PaintCanvas::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space) {
        m_spaceDown = false;
        if (!m_panning) viewport()->unsetCursor();
        event->accept(); return;
    }
    QGraphicsView::keyReleaseEvent(event);
}

void PaintCanvas::focusOutEvent(QFocusEvent *event)
{
    if (m_editing && m_area) m_area->finishInteraction();
    m_editing = false;
    m_spaceDown = m_panning = false;
    viewport()->unsetCursor();
    QGraphicsView::focusOutEvent(event);
}

bool PaintCanvas::viewportEvent(QEvent *event)
{
    if (m_area && (event->type() == QEvent::TabletPress || event->type() == QEvent::TabletMove ||
                   event->type() == QEvent::TabletRelease)) {
        auto *tablet = static_cast<QTabletEvent *>(event);
        QTabletEvent mapped(tablet->type(), tablet->pointingDevice(), sourcePosition(tablet->position()),
                            tablet->globalPosition(), tablet->pressure(), tablet->xTilt(), tablet->yTilt(),
                            tablet->tangentialPressure(), tablet->rotation(), tablet->z(), tablet->modifiers(),
                            tablet->button(), tablet->buttons());
        m_editing = tablet->type() != QEvent::TabletRelease && tablet->buttons() != Qt::NoButton;
        m_area->sendWorkspaceEvent(&mapped);
        event->accept();
        return true;
    }
    return QGraphicsView::viewportEvent(event);
}
