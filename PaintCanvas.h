#ifndef PaintCanvas_h
#define PaintCanvas_h

#include <QColor>
#include <QImage>
#include <QPainterPath>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsPathItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsRectItem>
#include <QGraphicsItemGroup>
#include <QElapsedTimer>
#include <QGraphicsItem>
//#include "BezierPath.h"
#include "BezierCurve.h"
#include "ScriptBreakdown.h"
#include "StrokeAttributeDockWidget.h"

#include "PaintTypes.h"
class BrushInterface;
#include "Worker.h"


// --- DrawingOverlay Begin
#include <QWidget>
#include <QMouseEvent>
#include <QThread>
class DrawingOverlay : public QWidget {
    Q_OBJECT
public:
    DrawingOverlay(QWidget *parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_TranslucentBackground);  // Enable transparency
        setAttribute(Qt::WA_StaticContents);  // Optimize for infrequent changes
        setAutoFillBackground(false);  // No background fill
        setStyleSheet("background: transparent;");  // Ensure no opaque bg
        setMouseTracking(true);  // For hover if needed
        // If using tablet: setAttribute(Qt::WA_TabletTracking, true);
    }

    void clearStroke() {
        m_path = QPainterPath();  // Reset path
        lines.clear();
        update();  // Trigger repaint
    }

    void addPoint(const QPointF &point) {
        if (m_path.isEmpty()) {
            m_path.moveTo(point);
        } else {
            m_path.lineTo(point);  // Or use quadTo for smoothing
        }
        update();  // Repaint overlay
    }

    void setStrokeProperties(const StrokeProperties &props){
        strokeProperties = props;
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        QPainter painter(this);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        // Set pen for variable width, color, etc. (from your strokeProperties)

        strokeProperties.foregroundColor = Qt::red;
        strokeProperties.maxWidth = 7;

        QPen pen(strokeProperties.foregroundColor, strokeProperties.maxWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.drawPath(m_path);

        painter.drawLine(0, 0, width(), height());

        for(int i=1; i<lines.size(); i++) {
            GameFusion::Vector3D &p1 = lines[i-1];
            GameFusion::Vector3D &p2 = lines[i];
            painter.drawLine(p1.x_, p1.y_, p2.x_, p2.y_);
        }
    }

    // Override mouse/tablet events to build path (example for mouse)
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            addPoint(event->pos());
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (event->buttons() & Qt::LeftButton) {
            GameFusion::Log().info() << "Overlay point "<<event->pos().x()<<" "<<event->pos().y()<<"\n";
            addPoint(event->pos());

            GameFusion::Vector3D v(event->pos().x(), event->pos().y(), 0);
            lines.push_back(v);
        }
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            emit strokeCompleted(lines);  // Signal to transfer to scene
            clearStroke();
        }
    }

    void createThreadWorker() {

        // Create new thread and worker for the stroke
        QThread* thread = new QThread(this);
        QString layerUuid = QUuid::createUuid().toString();
        int activeLayerIndex = 0;
        Worker* worker = new Worker(layerUuid, activeLayerIndex, 0, strokeProperties, 0);
        worker->moveToThread(thread);
        m_threads.append(thread);
        m_currentWorker = worker;

        connect(this, &DrawingOverlay::newPointAvailable, worker, &Worker::addPoint, Qt::QueuedConnection);
        connect(thread, &QThread::started, worker, &Worker::process);
        connect(worker, &Worker::resultReady, this, &DrawingOverlay::handleResult);
        connect(worker, &Worker::finished, worker, &QObject::deleteLater);
        connect(worker, &Worker::finished, thread, &QThread::quit);

        // connect finished -> custom slot with argument
        connect(thread, &QThread::finished, this, [this, thread]() {
            handleThreadFinished(thread);
        });
        connect(thread, &QThread::finished,
                thread, &QObject::deleteLater);


        thread->start();
    }

    void handleResult(const WorkerResults &workerResults, bool completedFinal);

    void handleThreadFinished(QThread* thread);

signals:
    void strokeCompleted(const std::vector<GameFusion::Vector3D>& lines);
    void newPointAvailable(const QPointF &pt, const float pressure);

private:
    QPainterPath m_path;  // Current stroke path
    StrokeProperties strokeProperties;
    std::vector<GameFusion::Vector3D> lines;

    //
    // ----- Thread and Bezier Compute from points BEGIN
    //
    Worker* m_currentWorker = nullptr;
    QList<QThread*> m_threads;
    WorkerResults computeResult;
    int computePointIndex=0;
};

// --- DrawingOverlay END

class IndexedLineItem : public QGraphicsLineItem {
public:
    IndexedLineItem(qreal x1, qreal y1, qreal x2, qreal y2, int index )
        :QGraphicsLineItem(x1, y1, x2, y2){
        pointIndex = index;
    }
    int pointIndex = -1;
};

/*
// In paintcanvas.h (or new header)
class LayerGroupItem : public QGraphicsItemGroup {
public:
    LayerGroupItem() {
        //setCacheMode(QGraphicsItem::ItemCoordinateCache);  // Or DeviceCoordinateCache for frequent view changes
        setCacheMode(QGraphicsItem::DeviceCoordinateCache);  // Or DeviceCoordinateCache for frequent view changes
        setHandlesChildEvents(false);  // Optimize if no per-stroke interaction needed
    }
    // Optional: Override paint() for custom hints
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override {
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);  // If cache kicks in
        QGraphicsItemGroup::paint(painter, option, widget);
    }
};

*/

// In paintcanvas.h (or new header)
class StrokeGroupItem : public QGraphicsItemGroup {
public:
    StrokeGroupItem() {
        setCacheMode(QGraphicsItem::ItemCoordinateCache);  // Or DeviceCoordinateCache for frequent view changes
        //setCacheMode(QGraphicsItem::DeviceCoordinateCache);  // Or DeviceCoordinateCache for frequent view changes
        setHandlesChildEvents(false);  // Optimize if no per-stroke interaction needed
    }
    // Optional: Override paint() for custom hints
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override {
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);  // If cache kicks in
        QGraphicsItemGroup::paint(painter, option, widget);
    }
};

class LayerGroupItem : public QGraphicsItemGroup {
public:
    explicit LayerGroupItem(bool enableCache = true, int batchSize = 100)
        : m_enableCache(enableCache), m_batchSize(batchSize), m_currentBatch(nullptr) {

        //enableCache = true;
        if (enableCache) {
            setCacheMode(QGraphicsItem::ItemCoordinateCache);  // Cache if enabled (for subgroups)
        }
        setHandlesChildEvents(false);  // Optimize: no per-child interaction needed
    }

    // Add a stroke (or any child item). Automatically batches into cached subgroups.
    void addStroke(QGraphicsItem* stroke) {
        if (!m_currentBatch || m_currentBatch->childItems().size() >= m_batchSize) {
            // Create a new cached batch group
            m_currentBatch = new LayerGroupItem(true, m_batchSize);  // Recursive: subgroups cache
            addToGroup(m_currentBatch);
        }
        m_currentBatch->addToGroup(stroke);
        // Optional: Enable cache on the stroke itself if not batching
        // stroke->setCacheMode(QGraphicsItem::ItemCoordinateCache);
    }

    // Override paint for custom hints (applied to this group and propagates)
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override {
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
        //QGraphicsItemGroup::paint(painter, option, widget);

        if (m_useRasterizedImage) {
            // Draw the pre-rasterized pixmap instead of children
            if (!m_rasterizedPixmap.isNull()) {
                painter->drawPixmap(m_pixmapOffset, m_rasterizedPixmap);
            }
            else {
                updateRasterizedImage();
                if (!m_rasterizedPixmap.isNull())
                    painter->drawPixmap(m_pixmapOffset, m_rasterizedPixmap);
            }
        } else {
            // Normal painting of children
            QGraphicsItemGroup::paint(painter, option, widget);
        }
    }

    void optimize() {

        for(int i=childItems().size()-10; i >= 0; i--){
            childItems()[i]->setVisible(false);
        }
    }

    // Toggle to use rasterized image for rendering
    void setUseRasterizedImage(bool enable) {
        if (enable == m_useRasterizedImage) return;

        if (enable) {
            // Update the rasterized image before hiding children
            //update();
            updateRasterizedImage();
            // Hide all children to prevent them from painting
            /*
            for (QGraphicsItem* child : childItems()) {
                child->hide();
            }
            */
            // Delete all children (recursively deletes subgroups and strokes)

        } else {
            // Show all children
            for (QGraphicsItem* child : childItems()) {
                child->show();
            }
            m_rasterizedPixmap = QPixmap();  // Clear the pixmap
            m_pixmapOffset = QPointF();
        }
        m_useRasterizedImage = enable;
        update();  // Trigger repaint
    }

protected:
    // Update the rasterized image by rendering the current state (with children visible)
    // Update the rasterized image by rendering the current state (with children visible)
    void updateRasterizedImage() {
        QRectF bounds = boundingRect();
        if (bounds.isEmpty()) return;

        // Create a transparent image
        QImage image(bounds.size().toSize(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);

        // Set up painter for rasterization
        QPainter p(&image);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.translate(-bounds.topLeft());

        // Prepare style option for rendering
        QStyleOptionGraphicsItem opt;
        opt.rect = bounds.toAlignedRect();
        opt.exposedRect = bounds;

        // Render the group (this will call paint recursively, but since m_useRasterizedImage is false here, it paints children)
        QGraphicsItemGroup::paint(&p, &opt, nullptr);

        // Store as pixmap with offset
        m_rasterizedPixmap = QPixmap::fromImage(image);
        m_pixmapOffset = bounds.topLeft();

        while (!childItems().isEmpty()) {
            delete childItems().first();
        }
    }



private:
    bool m_enableCache;
    int m_batchSize;
    LayerGroupItem* m_currentBatch;

    bool m_useRasterizedImage;
    QPixmap m_rasterizedPixmap;
    QPointF m_pixmapOffset;
};

// ----- Paint Canvas
class PaintCanvas : public QGraphicsView
{
    Q_OBJECT
public:
    enum class ToolMode {
        Paint,
        Select,
        Erase,
        Edit,
        Camera,
        Layer
    };
    enum class CameraAnimationMode { Linear, Smooth };
    PaintCanvas(QWidget *parent = 0);
    ~PaintCanvas();

    bool openImage(const QString &fileName);
    bool saveImage(const QString &fileName, const char *fileFormat);
    void setImage(const QImage &image);
    void insertShape(const QPainterPath &path);
    void setBrushColor(const QColor &color);
    void setBrushWidth(int width);
    void setBrush(BrushInterface *brushInterface, const QString &brush);
    //QImage image() const { return compositeImage; }
    QImage compositedImage() const;
    QColor brushColor() const { return color; }
    int brushWidth() const { return thickness; }
    QSize sizeHint() const override;

    // AJC
    void clearImage();
    void addNewCamera();
    void duplicateActiveCamera();
    void deleteActiveCamera();
    void renameActiveCamera();
    void editActiveCameraProperties();
    int findCameraUnderCursor(const QPoint& pos);
    void setPanel(const GameFusion::Panel &panel, const int startTime, const float fps, const GameFusion::CameraAnimation& cameraAnimation);
    void setPanel(const GameFusion::Panel &panel);
    void updateCameraFrameUI(const GameFusion::CameraFrame& frame);
    void setCurrentTime(const double currentTime);
    void startPlayback();
    void addNewLayer(); // New method for adding layers
    void updateCompositeImage();
    void setActiveLayer(const QString& uuid);
    void setLayerVisibility(const QString& uuid, bool visible);
    void updateLayer(const GameFusion::Layer &layer);
    void updateCamera(const GameFusion::CameraFrame &camera);

    //
    void setStrokeProperties(const StrokeProperties &props);
    void setToolMode(ToolMode mode);
    void setPlaybackMode(bool);
    void setFpsDisplay(bool enabled); // Toggle FPS display
    void setPipDisplay(bool enabled); // Toggle PIP display
    void invalidateAllLayers();
    SelectionSettings& selectionSettings();

    // zoom in/out feature
    void zoomIn(double factor = 1.25);
    void zoomOut(double factor = 1.25);
    void resetZoom();
    double zoomFactor() const { return m_zoomFactor; }
    void applyZoom(double factor);
    void fitToBase();

    void setProjectPath(QString p){projectPath=p;}
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void generateCameraThumbnail(const QString &cameraUuid, QImage &thumbnail);
    void drawCameraFrame(const GameFusion::CameraFrame& frame, QImage &croppedView);
    void renderFrameToImage(QImage &frameImage);
    void renderScene(QPainter& painter, bool isExportMode = false);
    void setExportMode(bool enabled) { m_exportMode = enabled; }
    bool isExportMode() const { return m_exportMode; }
    void setLightTableMode(bool enabled);
    // --- motion path functions
    QPointF sceneToScreen(double x, double y);
    void renderMotionPath(QPainter &painter);
    void updateMotionPaths(); // Compute motion paths for layers and cameras

    void addStrokeToScene(const std::vector<GameFusion::Vector3D> &stroke);

signals:
    void strokeSelected(const SelectionFrameUI& selectedStrokes);
    void cameraFrameUpdated(const GameFusion::CameraFrame& frame, bool editing);
    void cameraFrameAdded(const GameFusion::CameraFrame& frame);
    void cameraFrameDeleted(const QString& uuid);
    void layerAdded(const GameFusion::Layer &layer); // Signal for new layer
    void layerModified(const GameFusion::Layer &layer);
    void layerErasedStrokes(const GameFusion::Layer &layer);
    void layerThumbnailComputed(const QString& uuid, const QImage& thumbnail);
    void compositImageModified(const QString& panelUuid, const QImage& image, bool editing);
    void toolModeChanged(ToolMode mode); // Signal to notify mode change
    void keyFramePositionChanged(const QString& layerUuid, int keyFrameIndex, double oldX, double oldY, double newX, double newY);
    void LayerPositionChanged(const QString& layerUuid, double oldX, double oldY, double newX, double newY, bool isEditing);
    //bool pointOnSegment(const QPointF& p, const QPointF& s1, const QPointF& s2);

protected:
    void resizeEvent(QResizeEvent *event);
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent* event) override;
    void tabletEvent(QTabletEvent *event) override;
    void enterEvent(QEnterEvent *event) override; // Handle mouse/stylus entry
    void focusInEvent(QFocusEvent *event) override; // Optional: Debug focus gain
    void focusOutEvent(QFocusEvent *event) override; // Optional: Debug focus loss
    HandleContext findHandleUnderCursor(const QPoint &pos);
    void drawHandles(QPainter& painter, const FrameUI& frame, int cameraIndex);
    void drawBezierControls(QPainter &painter, LayerUI &layer);
    void drawBezierControls(QPainter &painter, LayerUI &layer, GameFusion::BezierCurve& curve);
    void setupPainter(QPainter &painter);
    float ApplyEasing(float t, GameFusion::EasingType easing);

private:
    QGraphicsScene *m_scene;
    //QVector<QGraphicsPixmapItem*> m_layerItems;
    QVector<LayerGroupItem*> m_layerItems;
    QGraphicsItem *m_tempStrokeItem = nullptr;
    QGraphicsRectItem *m_selectionItem = nullptr;
    QGraphicsEllipseItem *m_cursorItem = nullptr;
    QGraphicsPathItem *m_cameraMotionPathItem = nullptr;
    QVector<QGraphicsRectItem*> m_cameraFrameItems;
    QVector<QGraphicsItemGroup*> m_cameraHandleGroups;
    QVector<QGraphicsPathItem*> m_layerMotionPathItems;
    QVector<QGraphicsItemGroup*> m_layerMotionHandleGroups;
    QGraphicsItemGroup *m_editBezierGroup = nullptr;
    QList<LayerUI> layersUI;
    LayerUI copiedLayerUI;
    bool hasCopiedLayer = false;
    HandleContext currentHandleContext;
    //QList<Layer> layers; // List of layers
    int activeLayerIndex = 0; // Current layer for painting
    std::string activeLayerUuid;
    QImage compositeImage; // Precomposited image for display (for compatibility, but primarily use scene)
    QImage compositeLightTableImage; // Faded background composite (non-active layers only)
    //QImage theImage;
    QImage imageRef;
    QColor color;
    int thickness;
    BrushInterface *brushInterface;
    QString brush;
    QPoint lastPos;
    QPainterPath pendingPath;
    //QVector <Layer> layers;

public:
    QImage undoImage;
    QImage redoImage;
    //std::vector<QPointF> currentStrokePoints; // For brush and eraser
    std::vector<QPointF> eraserPath; // For eraser mode
    bool isDrawing = false;

private:
    GameFusion::CameraFrame cameraModelFromUI(const CameraFrameUI& uiFrame) const;
    //void computeLayerImage(LayerUI& layerUI, QPainter& painter);
    void computeLayerImage(LayerUI& layerUI);
    //QRectF drawBezierCurve(QPainter& painter, GameFusion::BezierCurve& curve);
    QRectF drawBezierCurve(LayerGroupItem* group, GameFusion::BezierCurve& curve);
    QImage generateThumbnail(const QImage& sourceImage, const QRectF& sourceBounds);
    void renderHDCameraView(QPainter& painter, const QRectF& cameraRect, qreal rotation, qreal zoom, long currentTimeMs, float fps);
    void prepareLayerImages();
    LayerValues computeLayerValuesAtTime(const GameFusion::Layer& layer, double timeMs, float fps);
    void setCameraAnimationMode(CameraAnimationMode cameraAnimationMode);

    CameraAnimationMode m_cameraAnimationMode = CameraAnimationMode::Linear;
    QString currentPanelUuid;
    QVector<CameraFrameUI*> cameraFrames;
    GameFusion::BezierCurve cameraMotionPathCurve;
    int hoveredHandleIndex = -1;
    int activeFrameIndex = -1;
    enum HandleType { None, Move, ResizeTL, ResizeTR, ResizeBL, ResizeBR, Rotate };
    HandleType currentHandle = None;
    QPointF mousePressOffset;
    double currentTime = 0; // current panel time in milliseconds
    // PaintArea.h
    CameraFrameUI copiedCameraFrameUI;
    bool hasCopiedCameraFrame = false;
    long startTime=0;
    float fps=25;
    bool isPlaying=false;
    // fps counter
    bool showFps = false;            // Enable/disable display
    bool showPip = true;
    int frameCount = 0;
    QElapsedTimer fpsTimer;
    float currentFps = 0.0f;
    bool vectorEditMode = true; // Flag for vector edit mode
    //
    StrokeProperties strokeProperties;
    ToolMode currentTool = ToolMode::Paint;
    QPoint cursorPos;
    bool showPaintCursor = false;
    // Selection Vars
    QRectF selectionRect; // For drawing selection in ToolMode::Select
    bool isSelecting = false; // Tracks if selection rect is being drawn
    bool isEditing = false;
    SelectionFrameUI currentSelection; // Current selection frame
    bool hasSelection = false; // Tracks if a selection is active
    QTransform layerTransform;
    QRectF strokeBounds;
    SelectionSettings m_selectionSettings;
    double m_zoomFactor = 1.0;
    QSize m_baseSize; // size of the original canvas/image
    QString projectPath;
    QString lastImagePath; // Tracks the last loaded image path for refImage
    bool m_exportMode = false;
    bool m_lightTableMode = false;  // New: Light table (onion-skin) mode

    // Interactive Motion Paths Context
    struct MotionHandleContext {
        QString uuid;
        int keyFrameIndex;
        QPointF position; // Screen coordinates
        bool isCamera=false; // Flag to distinguish camera keyframes and layers
    };

    std::vector<MotionHandleContext> motionHandleContext;
    bool updateMotionHandleContext = true;
    int draggedKeyFrameIndex = -1;
    QString draggedLayerUuid;
    double draggedOldX = 0.0;
    double draggedOldY = 0.0;
    std::pair<double, double> screenToScene(const QPointF& point);
    // -- motion path camera
    bool updateCameraMotionPath = true; // New: Flag to recompute camera path
    // tablet & pressure vars
    bool isTabletActive = false;
    float currentPressure = 1.0f; // Default for mouse
    std::vector<GameFusion::StrokePoint> currentStrokePoints;

    //
    // ----- Thread and Bezier Compute from points BEGIN
    //
    Worker* m_currentWorker = nullptr;
    QList<QThread*> m_threads;
    WorkerResults computeResult;
    int computePointIndex=0;

public:
    static void performLocalFit(FitResult& result, const std::vector<GameFusion::StrokePoint>& points, const StrokeProperties& props);

private slots:
    void handleResult(const WorkerResults &workerResults, bool completedFinal);
    void handleThreadFinished(QThread* thread);

signals:
    void newPointAvailable(const QPointF &pt, const float pressure);
    //
    // ----- Thread and Bezier Compute from points END
    //

private:
    static bool m_verbose;
    void updateTempStrokeItem();
    void updateEditBezierGroup();
    void updateCameraFrameItems();
    void updateLayerTransforms();

    void createReferenceBorder();

    void createThreadWorker();

    QGraphicsItemGroup* currentStrokeGroup = nullptr;

    DrawingOverlay *m_drawingOverlay;

};
#endif
