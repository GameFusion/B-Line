#include "PaintCanvas.h"

#include <QActionGroup>
#include <QGraphicsScene>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QScopedValueRollback>
#include <QTabletEvent>
#include <QToolBar>
#include <QToolButton>
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
    m_toolbar->setObjectName("workspaceToolbar");
    m_toolbar->setIconSize(QSize(18, 18));
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolbar->setStyleSheet(
        "QToolBar { background: #1D2330; border: 0; spacing: 4px; padding: 5px; }"
        "QToolButton { background: #1D2330; border: 1px solid #2E3950;"
        " border-radius: 6px; padding: 5px 7px; color: #D7DEEA; }"
        "QToolButton:hover { background: #273149; border-color: #3A4A6B; }"
        "QToolButton:checked { background: #0AA2FF; border-color: #3EC8FF; }"
        "QToolButton:pressed { background: #0587DA; }"
        "QLabel { color: #D7DEEA; padding: 0 5px; }"
        "QToolBar::separator { background: #3A4A6B; width: 1px; margin: 4px; }");
    m_tools->setExclusive(true);
    struct Tool { QString label; PaintArea::ToolMode mode; ushort glyph; };
    // Match the Font Awesome symbols used by MainWindowPaint::createTools.
    const QList<Tool> tools = {
        {tr("Draw"), PaintArea::ToolMode::Paint, 0xf1fc},
        {tr("Text"), PaintArea::ToolMode::Text, 0xf031},
        {tr("Select"), PaintArea::ToolMode::Select, 0xf245},
        {tr("Erase"), PaintArea::ToolMode::Erase, 0xf12d},
        {tr("Vector edit"), PaintArea::ToolMode::Edit, 0xf5ad},
        {tr("Camera"), PaintArea::ToolMode::Camera, 0xf03d},
        {tr("Layer"), PaintArea::ToolMode::Layer, 0xf5fd}};
    for (const auto &tool : tools) {
        QAction *action = addIconAction(tool.label, tool.glyph);
        action->setCheckable(true);
        action->setData(int(tool.mode));
        m_tools->addAction(action);
        connect(action, &QAction::triggered, this, [this, tool] {
            if (m_area && !m_editing) m_area->setToolMode(tool.mode);
            updateToolActions();
        });
    }
    m_toolbar->addSeparator();
    connect(addIconAction(tr("Zoom out"), 0xf010), &QAction::triggered,
            this, [this] { applyZoom(zoomFactor() / 1.25); });
    m_toolbar->addWidget(m_zoomLabel);
    connect(addIconAction(tr("Zoom in"), 0xf00e), &QAction::triggered,
            this, [this] { applyZoom(zoomFactor() * 1.25); });
    connect(addIconAction(tr("Actual size (100%)"), 0xf002), &QAction::triggered,
            this, [this] { applyZoom(1.0); });
    connect(addIconAction(tr("Fit canvas (F)"), 0xf31e), &QAction::triggered,
            this, &PaintCanvas::fitToBase);
    m_toolbar->addSeparator();
    m_lightTable = addIconAction(tr("Light table"), 0xf0eb);
    m_lightTable->setObjectName("workspaceLightTable");
    m_lightTable->setCheckable(true);
    // Feature toggles use the same amber accent as the integrated toolbar.
    m_toolbar->widgetForAction(m_lightTable)->setStyleSheet(
        "QToolButton:checked { background: #FF9F1C; border-color: #FFD28A; }");
    connect(m_lightTable, &QAction::toggled, this, [this](bool on) {
        if (m_area) m_area->setLightTableMode(on);
    });
    QAction *pip = addIconAction(tr("Camera preview"), 0xf108);
    pip->setCheckable(true);
    pip->setChecked(true);
    connect(pip, &QAction::toggled, this, [this](bool on) { m_showPip = on; viewport()->update(); });
    m_toolbar->setToolTip(tr("Wheel: zoom · Trackpad: pan · Space-drag or middle button: pan · F: fit"));
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, &PaintCanvas::invalidateDrawing);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &PaintCanvas::invalidateDrawing);
    updateZoomLabel();
}

QAction *PaintCanvas::addIconAction(const QString &label, ushort glyph)
{
    static const QString family = [] {
        const int id = QFontDatabase::addApplicationFont(":/fa-solid-900.ttf");
        return QFontDatabase::applicationFontFamilies(id).value(0);
    }();
    QIcon icon;
    if (!family.isEmpty()) {
        QFont font(family);
        font.setPixelSize(16);
        for (auto mode : {QIcon::Normal, QIcon::Disabled}) {
            for (auto state : {QIcon::Off, QIcon::On}) {
                QPixmap pixmap(36, 36);
                pixmap.setDevicePixelRatio(2);
                pixmap.fill(Qt::transparent);
                QPainter painter(&pixmap);
                painter.setRenderHint(QPainter::TextAntialiasing);
                painter.setFont(font);
                painter.setPen(mode == QIcon::Disabled ? QColor("#68758A") :
                               state == QIcon::On ? QColor(Qt::white) : QColor("#D7DEEA"));
                painter.drawText(QRectF(0, 0, 18, 18), Qt::AlignCenter, QChar(glyph));
                painter.end();
                icon.addPixmap(pixmap, mode, state);
            }
        }
    }
    QAction *action = m_toolbar->addAction(icon, label);
    action->setToolTip(label);
    m_toolbar->widgetForAction(action)->setAccessibleName(label);
    return action;
}

void PaintCanvas::updateLightTableAction()
{
    const QSignalBlocker block(m_lightTable);
    m_lightTable->setEnabled(m_area);
    m_lightTable->setChecked(m_area && m_area->lightTableMode());
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
        connect(area, &PaintArea::playbackViewChanged, this, &PaintCanvas::updatePlaybackView);
        connect(area, &PaintArea::toolModeChanged, this, &PaintCanvas::updateToolActions);
        connect(area, &PaintArea::lightTableModeChanged, this, &PaintCanvas::updateLightTableAction);
        connect(area, &QObject::destroyed, this, [this] {
            m_drawing = QPicture(); updateToolActions(); updateLightTableAction(); updatePlaybackView(); invalidateDrawing();
        });
    }
    m_drawing = QPicture();
    updateToolActions();
    updateLightTableAction();
    updatePlaybackView();
    invalidateDrawing();
    if (area && isVisible() && !m_fitted) fitToBase();
}

void PaintCanvas::setHistoryActions(QAction *undo, QAction *redo)
{
    m_toolbar->addSeparator();
    for (const auto &entry : {qMakePair(undo, ushort(0xf0e2)), qMakePair(redo, ushort(0xf01e))}) {
        QAction *source = entry.first;
        QAction *button = addIconAction(source->text(), entry.second);
        auto sync = [source, button] {
            button->setEnabled(source->isEnabled());
            button->setText(source->text());
            button->setToolTip(source->text());
        };
        connect(source, &QAction::changed, button, sync);
        connect(source, &QObject::destroyed, button, [button] { button->setEnabled(false); });
        connect(button, &QAction::triggered, source, &QAction::trigger);
        sync();
        addAction(source); // Preserve the shared Undo/Redo keyboard shortcuts.
    }
}

void PaintCanvas::updatePlaybackView()
{
    const bool enabled = !m_area || m_area->playbackViewEnabled(PaintArea::PlaybackView::Workspace);
    if (!enabled && viewport()->updatesEnabled() && isVisible()) {
        QScopedValueRollback<bool> capture(m_capturingPlaybackSnapshot, true);
        m_frozenPlaybackView = viewport()->grab();
    }
    if (enabled) m_frozenPlaybackView = QPixmap();
    viewport()->setUpdatesEnabled(enabled);
    m_dirty = true; // Catch up to the current frame when this view becomes active.
    if (enabled) viewport()->update();
}

void PaintCanvas::invalidateDrawing()
{
    m_dirty = true;
    if (viewport()->updatesEnabled()) viewport()->update(); // Qt coalesces updates.
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
        m_strokeBackgroundRect = source.mapRect(visible);
        const qreal density = zoomFactor() * viewport()->devicePixelRatioF() / m_sourceZoom;
        m_strokeBackground = m_area->strokeBackground(m_strokeBackgroundRect, density);
        m_drawing = m_area->workspacePicture(m_strokeBackgroundRect, density, m_strokeBackground.isNull());
    }
    painter->save();
    painter->scale(1.0 / m_sourceZoom, 1.0 / m_sourceZoom);
    if (!m_strokeBackground.isNull()) painter->drawImage(m_strokeBackgroundRect, m_strokeBackground);
    // QPicture uses a fixed logical DPI; compensate so model pixels stay pixels
    // on macOS, offscreen tests and HiDPI screens alike.
    painter->scale(qreal(m_drawing.logicalDpiX()) / painter->device()->logicalDpiX(),
                   qreal(m_drawing.logicalDpiY()) / painter->device()->logicalDpiY());
    painter->drawPicture(QPointF(), m_drawing);
    painter->restore();
}

void PaintCanvas::paintEvent(QPaintEvent *event)
{
    if (m_area && !m_capturingPlaybackSnapshot &&
        !m_area->playbackViewEnabled(PaintArea::PlaybackView::Workspace)) {
        QPainter painter(viewport());
        painter.fillRect(viewport()->rect(), Qt::white);
        painter.drawPixmap(QPoint(), m_frozenPlaybackView);
        return;
    }
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
    if (m_area) m_area->notifyPlaybackFramePainted(PaintArea::PlaybackView::Workspace);
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
