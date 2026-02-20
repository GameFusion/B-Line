#include <QPainter>
#include <QMouseEvent>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenu>
#include <QThread>

#include <QOpenGLWidget>
#include <QSurfaceFormat>

#include "FitCurves.h"
#include "List.h"
#include "Vector3D.h"
#include "Color.h"
#include "BezierPath.h"
#include "SoundDevice.h"

#include "PaintCanvas.h"

#include <QPainter>
#include <QPainterPath>
#include "BezierCurve.h"

bool PaintCanvas::m_verbose = false;

PaintCanvas::PaintCanvas(QWidget *parent) :
    QGraphicsView(parent),
    //theImage(1920, 1080, QImage::Format_RGB32),
    //theImage(3840, 2160, QImage::Format_RGB32),
    activeLayerIndex(0),
    //compositeImage(3840, 2160, QImage::Format_ARGB32_Premultiplied),
    compositeImage(1920, 1080, QImage::Format_ARGB32_Premultiplied),
    color(Qt::blue),
    thickness(3),
    brushInterface(0),
    lastPos(-1, -1),
    hoveredHandleIndex(-1),
    activeFrameIndex(-1),
    currentHandle(None),
    isPlaying(false),
    vectorEditMode(false),
    currentTool(ToolMode::Paint),
    showPaintCursor(false),
    isSelecting(false),
    hasSelection(false),
    //m_baseSize(3840, 2160)
    m_baseSize(1920, 1080)
{
    //setMouseTracking(true);
    //setAttribute(Qt::WA_AcceptTouchEvents, true);
    //setAttribute(Qt::WA_TabletTracking, true);
    //setFocusPolicy(Qt::StrongFocus); // Ensure PaintCanvas can receive focus

    // In PaintCanvas constructor:
    QSurfaceFormat format;
    format.setSamples(4);  // Enable 4x MSAA for smoother antialiasing (adjust based on GPU capability; 0 disables)
    format.setDepthBufferSize(24);  // Optional: For better depth handling if using 3D-like effects
    format.setStencilBufferSize(8);  // Optional: If needed for clipping/masks

    QOpenGLWidget *glWidget = new QOpenGLWidget();
    glWidget->setFormat(format);
    setViewport(glWidget);

    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);  // Or MinimalViewportUpdate for even less overhead (only redraws changed areas)
    setOptimizationFlags(QGraphicsView::DontSavePainterState | QGraphicsView::DontAdjustForAntialiasing);  // Reduces state changes and AA adjustments

#ifdef __APPLE__
    this->setAttribute(Qt::WA_NoSystemBackground);
#endif
    //mainWindow.setAttribute(Qt::WA_PaintOnScreen, true);


    setTabletTracking(true);
    setAttribute(Qt::WA_StaticContents);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    setAttribute(Qt::WA_NoBackground);
#else
    setAttribute(Qt::WA_OpaquePaintEvent);
#endif
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);
    m_scene->setBackgroundBrush(Qt::white);
    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::SmoothPixmapTransform, true);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setDragMode(QGraphicsView::NoDrag); // Set conditionally in setToolMode
    setSceneRect(-10000, -10000, 20000, 20000); // Large for infinite canvas
    m_tempStrokeItem = nullptr;
    m_selectionItem = new QGraphicsRectItem;
    m_selectionItem->setVisible(false);
    m_scene->addItem(m_selectionItem);
    m_cursorItem = new QGraphicsEllipseItem(-5, -5, 10, 10);
    m_cursorItem->setPen(QPen(Qt::red));
    m_cursorItem->setVisible(false);
    m_scene->addItem(m_cursorItem);
    m_cameraMotionPathItem = new QGraphicsPathItem;
    m_cameraMotionPathItem->setPen(QPen(Qt::cyan, 2, Qt::DashLine));
    m_scene->addItem(m_cameraMotionPathItem);
    m_editBezierGroup = new QGraphicsItemGroup;
    m_scene->addItem(m_editBezierGroup);
    // Initialize first layer
    GameFusion::Layer baseLayer;
    //baseLayer.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    baseLayer.name = "Layer BG";
    LayerUI baseLayerUI;
    baseLayerUI.image = QImage(m_baseSize, QImage::Format_ARGB32_Premultiplied);
    baseLayerUI.image.fill(Qt::transparent);
    baseLayerUI.rect = QRectF(0, 0, m_baseSize.width(), m_baseSize.height());
    baseLayerUI.layer = baseLayer;

    //layers.append(baseLayer);
    compositeImage.fill(Qt::transparent);


    LayerGroupItem *baseGroup = new LayerGroupItem;
    baseGroup->setZValue(layersUI.size() - 1);
    m_layerItems.append(baseGroup);
    m_scene->addItem(baseGroup);
    baseLayerUI.layerGroup = baseGroup;


    //QGraphicsPixmapItem *baseItem = new QGraphicsPixmapItem;
    //m_layerItems.append(baseItem);
    //m_scene->addItem(baseItem);
    //baseItem->setZValue(0);
    //theImage.fill(qRgb(255, 255, 255));

    layersUI.append(baseLayerUI);

    // test camera frame
    bool CameraFrameUnitTest = false;
    if(CameraFrameUnitTest){
        GameFusion::CameraFrame frame;
        frame.name = "Camera 1";
        cameraFrames.append(new CameraFrameUI(QRectF(100, 100, 1920 / 5, 1080 / 5), frame));
        QGraphicsRectItem *camItem = new QGraphicsRectItem(cameraFrames.last()->rect);
        camItem->setPen(QPen(Qt::green));
        m_cameraFrameItems.append(camItem);
        m_scene->addItem(camItem);
        QGraphicsItemGroup *handleGroup = new QGraphicsItemGroup;
        m_cameraHandleGroups.append(handleGroup);
        m_scene->addItem(handleGroup);
        // add handles to group
    }
    // Add shortcut for new layer (Ctrl+L)
    QAction* addLayerAction = new QAction(tr("Add New Layer"), this);
    addLayerAction->setShortcut(QKeySequence("Ctrl+L"));
    connect(addLayerAction, &QAction::triggered, this, &PaintCanvas::addNewLayer);
    addAction(addLayerAction);
    // Initialize default stroke properties
    strokeProperties.smoothness = 0.5f;
    strokeProperties.maxWidth = 2.0;
    strokeProperties.minWidth = 0.5;
    strokeProperties.variableWidthMode = StrokeProperties::Uniform;
    strokeProperties.stepCount = 20;

    createReferenceBorder();

    resize(m_baseSize);
    updateCompositeImage();

    m_drawingOverlay = new DrawingOverlay(viewport());  // Parent to viewport
    m_drawingOverlay->setGeometry(viewport()->geometry());  // Match size
    connect(m_drawingOverlay, &DrawingOverlay::strokeCompleted, this, &PaintCanvas::addStrokeToScene);
}

PaintCanvas::~PaintCanvas() {
    for (QThread* thread : m_threads) {
        thread->quit();
        thread->wait();
        delete thread; // Deletes worker as well
    }
    m_threads.clear();

    if (currentStrokeGroup) {
        delete currentStrokeGroup;
        currentStrokeGroup = nullptr;
    }
}

void PaintCanvas::createReferenceBorder(){
    // Add reference border frame (black, solid, 2px)
    QPen borderPen(Qt::black, 2, Qt::SolidLine);
    QGraphicsRectItem *borderItem = new QGraphicsRectItem(0, 0, m_baseSize.width(), m_baseSize.height());
    borderItem->setPen(borderPen);
    borderItem->setBrush(Qt::NoBrush);
    borderItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
    borderItem->setZValue(-100);  // Low z to be background
    m_scene->addItem(borderItem);

    // Add action safe frame (inset 5%, black, dashed, 2px)
    qreal insetX = m_baseSize.width() * 0.05;
    qreal insetY = m_baseSize.height() * 0.05;
    QRectF safeRect(insetX, insetY, m_baseSize.width() * 0.9, m_baseSize.height() * 0.9);
    QPen safePen(Qt::black, 2, Qt::DashLine);
    QGraphicsRectItem *safeItem = new QGraphicsRectItem(safeRect);
    safeItem->setPen(safePen);
    safeItem->setBrush(Qt::NoBrush);
    safeItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
    safeItem->setZValue(-99);  // Slightly above border
    m_scene->addItem(safeItem);
}

bool PaintCanvas::openImage(const QString &fileName)
{
    QImage image;
    if (!image.load(fileName))
        return false;
    setImage(image);
    return true;
}

bool PaintCanvas::saveImage(const QString &fileName, const char *fileFormat)
{
    //return theImage.save(fileName, fileFormat);
    return compositedImage().save(fileName, fileFormat);
}

QImage PaintCanvas::compositedImage() const {
    QImage img(sceneRect().size().toSize(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    m_scene->render(&p);
    return img;
}

void PaintCanvas::setImage(const QImage &image)
{
    //theImage = image.convertToFormat(QImage::Format_RGB32);
    imageRef = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    updateCompositeImage();
    viewport()->update();
    updateGeometry();
}

void PaintCanvas::insertShape(const QPainterPath &path)
{
    pendingPath = path;
#ifndef QT_NO_CURSOR
    setCursor(Qt::CrossCursor);
#endif
}

void PaintCanvas::setBrushColor(const QColor &color)
{
    this->color = color;
}

void PaintCanvas::setBrushWidth(int width)
{
    thickness = width;
}

void PaintCanvas::setBrush(BrushInterface *brushInterface, const QString &brush)
{
    this->brushInterface = brushInterface;
    this->brush = brush;
}

QSize PaintCanvas::sizeHint() const
{
    //return theImage.size();
    return compositeImage.size();
}

// In PaintCanvas.cpp: Refactor addNewLayer (remove raster code)
void PaintCanvas::addNewLayer() {
    LayerUI newLayerUI;
    newLayerUI.layer.name = QString("Layer %1").arg(layersUI.size() + 1).toStdString();
    newLayerUI.rect = QRectF(0, 0, m_baseSize.width(), m_baseSize.height());

    LayerGroupItem *newGroup = new LayerGroupItem;
    newGroup->setZValue(layersUI.size() - 1);
    m_layerItems.append(newGroup);
    m_scene->addItem(newGroup);
    newLayerUI.layerGroup = newGroup;

    layersUI.append(newLayerUI);
    activeLayerIndex = layersUI.size() - 1;

    emit layerAdded(newLayerUI.layer);
    viewport()->update();  // No updateCompositeImage needed
}



QRectF PaintCanvas::drawBezierCurve(LayerGroupItem* layerGroup, GameFusion::BezierCurve& curve)
{
    // Ensure the curve is assessed to generate up-to-date vertices and align with pre-computed pressures
    // This guarantees vertexArray() and strokePressure() are synchronized (no re-sampling mismatch)
    StrokeProperties curveProperties = curve.getStrokeProperties();
    if(curve.vertexArray().empty())
        curve.assess(curveProperties.stepCount, false);  // Generate vertices at stepCount resolution; false = not closed
    // Initialize bounding rectangle from vertices (more accurate than path approximation)
    QRectF boundingRect;
    const std::vector<GameFusion::Vector3D>& vertexArray = curve.vertexArray();
    if (!vertexArray.empty()) {
        // Compute initial bbox from first vertex (zoomed)
        const GameFusion::Vector3D& firstVert = vertexArray[0];
        boundingRect = QRectF(firstVert.x(), firstVert.y(), 0, 0);
        // Expand to include all vertices (zoomed)
        for (const auto& vert : vertexArray) {
            boundingRect = boundingRect.united(QRectF(
                vert.x(), vert.y(), 1, 1
                ));
        }
    }
    // Early exit if no vertices
    if (vertexArray.empty()) {
        return boundingRect;
    }
    // Determine if variable width is needed (based on mode)
    bool variableWidth = false;
    float uniformWidth = curveProperties.maxWidth;  // Default for uniform
    switch (curveProperties.variableWidthMode) {
    case StrokeProperties::Uniform:
        variableWidth = false;
        break;
    case StrokeProperties::TaperIn:
    case StrokeProperties::TaperOut:
    case StrokeProperties::Pressure:
        variableWidth = true;
        break;
    }
    // Set up base pen properties (color/gradient handled below)
    QPen pen(curveProperties.foregroundColor, uniformWidth,  // Start with uniform; override width per-segment if variable
             Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    pen.setBrush(Qt::NoBrush);  // Lines, no fill
    // Handle color/gradient (independent of width)
    QPointF startPoint(vertexArray.front().x(), vertexArray.front().y());
    QPointF endPoint(vertexArray.back().x(), vertexArray.back().y());
    QLinearGradient gradient(startPoint, endPoint);
    switch (curveProperties.colorMode) {
    case StrokeProperties::SolidForeground:
        pen.setColor(curveProperties.foregroundColor);
        break;
    case StrokeProperties::SolidBackground:
        pen.setColor(curveProperties.backgroundColor);
        break;
    case StrokeProperties::GradientBGtoFG:
        gradient.setColorAt(0, curveProperties.backgroundColor);
        gradient.setColorAt(1, curveProperties.foregroundColor);
        pen.setBrush(gradient);
        break;
    case StrokeProperties::GradientFGtoBG:
        gradient.setColorAt(0, curveProperties.foregroundColor);
        gradient.setColorAt(1, curveProperties.backgroundColor);
        pen.setBrush(gradient);
        break;
    }
    // Reference to pre-computed pressures (sized to match vertexArray)
    const std::vector<float>& pressures = curve.strokePressure();
    bool hasPressures = !pressures.empty() && pressures.size() == vertexArray.size();
    // For uniform width: Draw the entire curve as a single path (smooth and efficient)

    StrokeGroupItem *strokeGroup = new StrokeGroupItem;

    if (!variableWidth) {
        // Build QPainterPath from control points (zoomed) for exact Bézier rendering
        QPainterPath path;
        if (curve.size() >= 1) {
            const auto& firstHandle = curve[0];
            path.moveTo(firstHandle.point.x(), firstHandle.point.y());
            for (size_t i = 0; i < curve.size() - 1; ++i) {
                const auto& current = curve[i];
                const auto& next = curve[i + 1];
                path.cubicTo(
                    (current.point.x() + current.rightControl.x()),
                    (current.point.y() + current.rightControl.y()),
                    (next.point.x() + next.leftControl.x()),
                    (next.point.y() + next.leftControl.y()),
                    next.point.x(),
                    next.point.y()
                    );
            }



            // Apply uniform width and draw
            pen.setWidthF(uniformWidth);
            QGraphicsPathItem *pathItem = new QGraphicsPathItem(path);
            pathItem->setPen(pen);  // Uniform pen for all
            strokeGroup->addToGroup(pathItem);

        }
        //painter.setPen(pen);
        //painter.drawPath(path);
    } else {
        // For variable width: Draw line segments between pre-computed vertices
        // This exactly matches the computed pressures (no interpolation mismatch)
        // Use averaged pressure per segment for smooth width transitions
        for (size_t i = 1; i < vertexArray.size(); ++i) {
            // Compute segment width
            float currentWidth;
            if (hasPressures && curveProperties.variableWidthMode == StrokeProperties::Pressure) {
                // Use pre-computed pressures: Average between adjacent vertices for fluid joins
                float p1 = pressures[i - 1];
                float p2 = pressures[i];
                float avgPressure = (p1 + p2) / 2.0f;
                currentWidth = curveProperties.minWidth + (curveProperties.maxWidth - curveProperties.minWidth) * avgPressure;
            } else {
                // Fallback for TaperIn/Out: Linear interpolation based on global t
                // (Compute uniform t from vertex index; matches assess() parameterization)
                float global_t = static_cast<float>(i - 1) / static_cast<float>(vertexArray.size() - 1);
                if (curveProperties.variableWidthMode == StrokeProperties::TaperIn) {
                    currentWidth = curveProperties.minWidth + (curveProperties.maxWidth - curveProperties.minWidth) * global_t;
                } else if (curveProperties.variableWidthMode == StrokeProperties::TaperOut) {
                    currentWidth = curveProperties.maxWidth + (curveProperties.minWidth - curveProperties.maxWidth) * global_t;
                } else {
                    // Parabolic fallback if no pressures (Pressure mode without data)
                    float tNorm = 2.0f * global_t - 1.0f;
                    currentWidth = curveProperties.minWidth + (curveProperties.maxWidth - curveProperties.minWidth) * (1.0f - tNorm * tNorm);
                }
            }
            // Update pen for this segment (zoom applied)
            pen.setWidthF(currentWidth);
            //painter.setPen(pen);
            // Draw line between zoomed vertices
            const GameFusion::Vector3D& v1 = vertexArray[i - 1];
            const GameFusion::Vector3D& v2 = vertexArray[i];
            /*
            painter.drawLine(
                QPointF(v1.x(), v1.y()),
                QPointF(v2.x(), v2.y())
                );
            */
            QGraphicsLineItem *line = new QGraphicsLineItem(
                v1.x(), v1.y(),
                v2.x(), v2.y()
                );
            line->setPen(pen);
            strokeGroup->addToGroup(line);
        }
    }
    // Expand bounding rectangle to account for max stroke width (zoomed)
    boundingRect.adjust(
        -curveProperties.maxWidth,
        -curveProperties.maxWidth,
        curveProperties.maxWidth,
        curveProperties.maxWidth
        );

    layerGroup->addToGroup(strokeGroup);

    //layerGroup->optimize();
    layerGroup->setUseRasterizedImage(true);

    return boundingRect;
}

void PaintCanvas::computeLayerImage(LayerUI& layerUI) {

    LayerGroupItem* group = layerUI.layerGroup;
    if (!group) return;

    // Clear existing children to rebuild
    QList<QGraphicsItem*> children = group->childItems();
    for (QGraphicsItem* child : children) {
        group->removeFromGroup(child);
        delete child;
    }

    for (GameFusion::BezierCurve& curve : layerUI.layer.strokes) {
        QRectF curveBounds = drawBezierCurve(group, curve);

        if(layerUI.rect.isNull())
            layerUI.rect = curveBounds;
        else
            layerUI.rect = layerUI.rect.united(curveBounds);        // extend bounding box
    }

    layerUI.imageDirty = false;
}



// Keep updateCompositeImage: Now rebuilds dirty groups + applies transforms/opacity
void PaintCanvas::updateCompositeImage() {
    for (int i = 0; i < layersUI.size(); ++i) {
        LayerUI& layerUI = layersUI[i];
        if (layerUI.imageDirty) {
            computeLayerImage(layerUI);  // Rebuilds vector group
        }

        LayerGroupItem* group = layerUI.layerGroup;

        //return;

        QTransform t = QTransform();// = layerUI.getTransform();  // Assuming no m_zoomFactor now, as view scales
        group->setTransform(t);
        float op = layerUI.layer.opacity;
        if (m_lightTableMode && i != activeLayerIndex) op *= 0.3f;
        group->setOpacity(op);
        group->setVisible(layerUI.layer.visible);
    }
}

void PaintCanvas::resizeEvent(QResizeEvent *event) {
    QGraphicsView::resizeEvent(event);
    if (m_drawingOverlay) {
        m_drawingOverlay->setGeometry(viewport()->geometry());
    }
}

void PaintCanvas::paintEvent(QPaintEvent *event) {
    static CameraFrameUI defaultCamera = []() {
        CameraFrameUI cam;
        cam.rect = QRectF(0, 0, 1920, 1080);  // HD resolution
        cam.frame.zoom = 1.0f;
        cam.frame.rotation = 0.0f;
        cam.frame.x = 0.0f;
        cam.frame.y = 0.0f;
        return cam;
    }();

    QGraphicsView::paintEvent(event);
    QPainter p(viewport());
    showFps = false;
    if (showFps) {
        p.setPen(Qt::black);
        p.drawText(10, 20, QString("FPS: %1").arg(currentFps));
    }
    if (showPip) {
        //CameraFrameUI interpolatedCamera = getInterpolatedCamera(currentTime);
        //CameraFrameUI &currentCamera = cameraFrames.isEmpty() ? defaultCamera : interpolatedCamera;

        CameraFrameUI &currentCamera = defaultCamera;

        float pipWidth = m_baseSize.width() / 9.0;
        float pipHeight = m_baseSize.height() / 9.0;

        QRect pipRect(width() - pipWidth -50, height() - pipHeight - 50, pipWidth, pipHeight);
        QImage pipImg(m_baseSize, QImage::Format_ARGB32_Premultiplied);
        pipImg.fill(Qt::transparent);
        QPainter pipP(&pipImg);
        pipP.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

        QPointF center = currentCamera.rect.center();
        qreal rotation = currentCamera.frame.rotation;
        qreal camZoom = currentCamera.frame.zoom;

        qreal fitScale = 1.0; // qMin(pipImg.width() / currentCamera.rect.width(), pipImg.height() / currentCamera.rect.height());

        QTransform V;
        double output_center_x = pipImg.width() / 2.0;
        double output_center_y = pipImg.height() / 2.0;
        V.translate(output_center_x, output_center_y);
        V.rotate(rotation);
        V.scale(camZoom, camZoom);
        V.translate(-center.x(), -center.y());

        pipP.setTransform(V);

        QRectF target(0, 0, pipImg.width(), pipImg.height());
       // QRectF source = V.inverted().mapRect(target);
        QRectF source(0, 0, m_baseSize.width(), m_baseSize.height());

        m_scene->render(&pipP, source, target);

        QImage imgScalled = pipImg.scaled(pipRect.width(), pipRect.height());
        p.drawImage(pipRect.topLeft(), imgScalled);
    }
}

void PaintCanvas::setLightTableMode(bool enabled) {
    m_lightTableMode = enabled;
    updateCompositeImage();
    viewport()->update();
}

// Implement other methods similarly, adapting to scene items where necessary.
void PaintCanvas::updateMotionPaths() {
    motionHandleContext.clear();
    // Clear existing motion path items and handles
    for (QGraphicsPathItem *pathItem : m_layerMotionPathItems) {
        m_scene->removeItem(pathItem);
        delete pathItem;
    }
    m_layerMotionPathItems.clear();
    for (QGraphicsItemGroup *group : m_layerMotionHandleGroups) {
        m_scene->removeItem(group);
        delete group;
    }
    m_layerMotionHandleGroups.clear();
    // Layers motion paths
    for (const auto& layerUI : layersUI) {
        const GameFusion::Layer& layer = layerUI.layer;
        QString uuid = QString::fromStdString(layer.uuid);
        QPainterPath path;
        if (!layer.motionKeyframes.empty()) {
            auto kf = layer.motionKeyframes[0];
            path.moveTo(kf.x, kf.y);
            for (size_t i = 1; i < layer.motionKeyframes.size(); ++i) {
                kf = layer.motionKeyframes[i];
                path.lineTo(kf.x, kf.y);
            }
        }
        QGraphicsPathItem *pathItem = new QGraphicsPathItem(path);
        pathItem->setPen(QPen(Qt::magenta, 1, Qt::DashLine));
        pathItem->setVisible(!m_exportMode);
        m_scene->addItem(pathItem);
        m_layerMotionPathItems.append(pathItem);
        for (int i = 0; i < layer.motionKeyframes.size(); ++i) {
            double x = layer.motionKeyframes[i].x;
            double y = layer.motionKeyframes[i].y;
            QPointF scenePos(x, y);
            QPointF screenPos = sceneToScreen(x, y);
            motionHandleContext.push_back({uuid, i, screenPos, false});
            QGraphicsItemGroup *group = new QGraphicsItemGroup;
            QGraphicsEllipseItem *handle = new QGraphicsEllipseItem(-5, -5, 10, 10);
            handle->setPos(screenPos);
            handle->setBrush(Qt::yellow);
            group->addToGroup(handle);
            m_scene->addItem(group);
            m_layerMotionHandleGroups.append(group);
        }
    }
    // Cameras motion path
    m_cameraMotionPathItem->setPath(QPainterPath());
    for (QGraphicsRectItem *frameItem : m_cameraFrameItems) {
        m_scene->removeItem(frameItem);
        delete frameItem;
    }
    m_cameraFrameItems.clear();
    for (QGraphicsItemGroup *group : m_cameraHandleGroups) {
        m_scene->removeItem(group);
        delete group;
    }
    m_cameraHandleGroups.clear();
    cameraMotionPathCurve.clear();
    std::vector<GameFusion::StrokePoint> cameraPoints;
    QPainterPath camPath;
    if (!cameraFrames.isEmpty()) {
        QPointF first = cameraFrames[0]->rect.center();
        camPath.moveTo(first.x(), first.y());
    }
    for (size_t i = 0; i < cameraFrames.size(); ++i) {
        double x = cameraFrames[i]->rect.center().x();
        double y = cameraFrames[i]->rect.center().y();
        QPointF screenPos = sceneToScreen(x, y);
        QString camUuid = QString::fromStdString(cameraFrames[i]->frame.uuid);
        motionHandleContext.push_back({camUuid, (int)i, screenPos, true});
        cameraPoints.push_back({QPointF(x, y), 1.0f});
        cameraMotionPathCurve += GameFusion::BezierControl({(float)x, (float)y, 0}, {0,0, 0}, {0,0, 0}, i);
        camPath.lineTo(screenPos);
        QRectF scaledRect = QRectF(
            cameraFrames[i]->rect.x(),
            cameraFrames[i]->rect.y(),
            cameraFrames[i]->rect.width(),
            cameraFrames[i]->rect.height()
            );
        QGraphicsRectItem *frameItem = new QGraphicsRectItem(scaledRect); // scale rect?
        frameItem->setTransform(cameraFrames[i]->getTransform(1));
        frameItem->setPen(QPen(Qt::green));
        m_scene->addItem(frameItem);
        m_cameraFrameItems.append(frameItem);
        QGraphicsItemGroup *group = new QGraphicsItemGroup;
        // add handles
        QVector<QPointF> handles = cameraFrames[i]->getHandlePositions();
        for (QPointF h : handles) {
            QGraphicsEllipseItem *handleItem = new QGraphicsEllipseItem(-5, -5, 10, 10);
            handleItem->setPos(h);
            group->addToGroup(handleItem);
        }
        m_scene->addItem(group);
        m_cameraHandleGroups.append(group);
    }
    cameraMotionPathCurve.assess(20, false);
    m_cameraMotionPathItem->setPath(camPath);
    m_cameraMotionPathItem->setVisible(!m_exportMode);
}

QPointF PaintCanvas::sceneToScreen(double x, double y) {
    return QPointF(x * m_zoomFactor, y * m_zoomFactor);
}

std::pair<double, double> PaintCanvas::screenToScene(const QPointF& point) {
    return {point.x() / m_zoomFactor, point.y() / m_zoomFactor};
}

void PaintCanvas::zoomIn(double factor) {
    scale(factor, factor);
    m_zoomFactor *= factor;
    //updateLayerTransforms();
    //updateMotionPaths();
    updateCameraFrameItems();
    viewport()->update();
}

void PaintCanvas::zoomOut(double factor) {
    scale(1.0 / factor, 1.0 / factor);
    m_zoomFactor /= factor;
    updateLayerTransforms();
    updateMotionPaths();
    updateCameraFrameItems();
    viewport()->update();
}

void PaintCanvas::resetZoom() {
    scale(1.0 / m_zoomFactor, 1.0 / m_zoomFactor);
    m_zoomFactor = 1.0;
    updateLayerTransforms();
    updateMotionPaths();
    updateCameraFrameItems();
    viewport()->update();
}

void PaintCanvas::applyZoom(double factor) {
    scale(factor / m_zoomFactor, factor / m_zoomFactor);
    m_zoomFactor = factor;
    updateLayerTransforms();
    updateMotionPaths();
    updateCameraFrameItems();
    viewport()->update();
}

void PaintCanvas::updateLayerTransforms() {
    /*
    for (int i = 0; i < layersUI.size(); ++i) {
        QTransform t = layersUI[i].getTransform(m_zoomFactor);
        m_layerItems[i]->setTransform(t);
    }
    */
}

void PaintCanvas::updateCameraFrameItems() {
    /*
    for (int i = 0; i < cameraFrames.size(); ++i) {
        QTransform t = cameraFrames[i]->getTransform(m_zoomFactor);
        m_cameraFrameItems[i]->setTransform(t);
        // update handles pos too
        QVector<QPointF> handles = cameraFrames[i]->getHandlePositions();
        QList<QGraphicsItem*> children = m_cameraHandleGroups[i]->childItems();
        for (int j = 0; j < children.size(); ++j) {
            children[j]->setPos(handles[j]);
        }
    }
    */
}

void PaintCanvas::setToolMode(ToolMode mode) {
    currentTool = mode;
    switch (mode) {
    case ToolMode::Select:
        setDragMode(QGraphicsView::RubberBandDrag);
        break;
    case ToolMode::Paint:
    case ToolMode::Erase:
        setDragMode(QGraphicsView::NoDrag);
        break;
    case ToolMode::Camera:
    case ToolMode::Layer:
        setDragMode(QGraphicsView::ScrollHandDrag);
        break;
    case ToolMode::Edit:
        setDragMode(QGraphicsView::NoDrag);
        updateEditBezierGroup();
        break;
    }
    emit toolModeChanged(mode);
    viewport()->update();
}

void PaintCanvas::updateTempStrokeItem() {
    if (m_tempStrokeItem) {
        m_scene->removeItem(m_tempStrokeItem);
        delete m_tempStrokeItem;
        m_tempStrokeItem = nullptr;
    }
    if (computeResult.curve.empty()) return;
    StrokeProperties props = computeResult.curve.getStrokeProperties();
    bool variableWidth = props.variableWidthMode != StrokeProperties::Uniform;
    if (!variableWidth) {
        QPainterPath path;
        // build path similar to drawBezierCurve, with * m_zoomFactor
        const auto& curve = computeResult.curve;
        if (curve.size() >= 1) {
            const auto& first = curve[0];
            path.moveTo(first.point.x(), first.point.y());
            for (size_t i = 0; i < curve.size() - 1; ++i) {
                const auto& curr = curve[i];
                const auto& next = curve[i + 1];
                path.cubicTo(
                    (curr.point.x() + curr.rightControl.x()),
                    (curr.point.y() + curr.rightControl.y()),
                    (next.point.x() + next.leftControl.x()),
                    (next.point.y() + next.leftControl.y()),
                    next.point.x(),
                    next.point.y()
                    );
            }
        }
        QGraphicsPathItem *pathItem = new QGraphicsPathItem(path);
        QPen pen(props.foregroundColor, props.maxWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        pathItem->setPen(pen);
        m_scene->addItem(pathItem);
        m_tempStrokeItem = pathItem;
    } else {
        QGraphicsItemGroup *group = new QGraphicsItemGroup;
        const auto& verts = computeResult.curve.vertexArray();
        const auto& pressures = computeResult.curve.strokePressure();
        for (size_t i = 1; i < verts.size(); ++i) {
            float width = props.maxWidth; // compute as in drawBezierCurve
            if (props.variableWidthMode == StrokeProperties::Pressure && pressures.size() == verts.size()) {
                float avgP = (pressures[i-1] + pressures[i]) / 2.0f;
                width = (props.minWidth + (props.maxWidth - props.minWidth) * avgP) * m_zoomFactor;
            } // or taper etc.
            QPen pen(props.foregroundColor, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            QGraphicsLineItem *line = new QGraphicsLineItem(
                verts[i-1].x(), verts[i-1].y(),
                verts[i].x(), verts[i].y()
                );
            line->setPen(pen);
            group->addToGroup(line);
        }
        m_scene->addItem(group);
        m_tempStrokeItem = group;
    }
}

void PaintCanvas::handleResult(const WorkerResults &workerResults, bool completedFinal) {
    computeResult = workerResults;
    if (completedFinal) {
        //computeResult.curve.assess(strokeProperties.stepCount, false);
        GameFusion::Layer* currentLayer = &layersUI[activeLayerIndex].layer;
        currentLayer->strokes.push_back(workerResults.curve);
        layersUI[activeLayerIndex].imageDirty = true;

        layersUI[activeLayerIndex].layerGroup->show();

        //updateTempStrokeItem();
        if(m_tempStrokeItem){
            m_scene->removeItem(m_tempStrokeItem);
            delete m_tempStrokeItem;
            m_tempStrokeItem = nullptr;
        }

        computeResult = WorkerResults(); // clear
        currentStrokePoints.clear();

        updateCompositeImage();
        emit layerModified(layersUI[activeLayerIndex].layer);

        viewport()->update();
        update();

    } else {

        // Prune lines with pointIndex <= workerResults.pointIndex
        if (currentStrokeGroup) {
            QList<QGraphicsItem*> children = currentStrokeGroup->childItems();
            for (QGraphicsItem* child : children) {
                IndexedLineItem* line = dynamic_cast<IndexedLineItem*>(child);
                if (line && line->pointIndex <= workerResults.pointIndex) {
                    currentStrokeGroup->removeFromGroup(child);
                    delete child;
                }
                else
                    break;
            }
        }

        updateTempStrokeItem();
    }
    viewport()->update(); // Trigger repaint
}

// Implement other slots and methods similarly.
void PaintCanvas::updateEditBezierGroup() {
    // Clear group
    QList<QGraphicsItem*> children = m_editBezierGroup->childItems();
    for (QGraphicsItem *item : children) {
        m_editBezierGroup->removeFromGroup(item);
        delete item;
    }
    if (currentTool != ToolMode::Edit || activeLayerIndex < 0) return;
    LayerUI &layer = layersUI[activeLayerIndex];
    for (GameFusion::BezierCurve &curve : layer.layer.strokes) {
        // Add lines for controls, ellipses for points and controls
        for (size_t i = 0; i < curve.size(); ++i) {
            const auto& control = curve[i];
            QPointF p(control.point.x() * m_zoomFactor, control.point.y() * m_zoomFactor);
            QPointF left(control.point.x() + control.leftControl.x() * m_zoomFactor, control.point.y() + control.leftControl.y() * m_zoomFactor);
            QPointF right(control.point.x() + control.rightControl.x() * m_zoomFactor, control.point.y() + control.rightControl.y() * m_zoomFactor);
            QGraphicsLineItem *leftLine = new QGraphicsLineItem(p.x(), p.y(), left.x(), left.y());
            leftLine->setPen(QPen(Qt::blue, 1));
            m_editBezierGroup->addToGroup(leftLine);
            QGraphicsLineItem *rightLine = new QGraphicsLineItem(p.x(), p.y(), right.x(), right.y());
            rightLine->setPen(QPen(Qt::blue, 1));
            m_editBezierGroup->addToGroup(rightLine);
            QGraphicsEllipseItem *pointHandle = new QGraphicsEllipseItem(-3, -3, 6, 6);
            pointHandle->setPos(p);
            pointHandle->setBrush(Qt::red);
            m_editBezierGroup->addToGroup(pointHandle);
            QGraphicsEllipseItem *leftHandle = new QGraphicsEllipseItem(-3, -3, 6, 6);
            leftHandle->setPos(left);
            leftHandle->setBrush(Qt::green);
            m_editBezierGroup->addToGroup(leftHandle);
            QGraphicsEllipseItem *rightHandle = new QGraphicsEllipseItem(-3, -3, 6, 6);
            rightHandle->setPos(right);
            rightHandle->setBrush(Qt::green);
            m_editBezierGroup->addToGroup(rightHandle);
        }
    }
}

void PaintCanvas::setCurrentTime(const double currentTime) {
    this->currentTime = currentTime;
    // Update layer values and transforms
    for (int i = 0; i < layersUI.size(); ++i) {
        LayerValues vals = computeLayerValuesAtTime(layersUI[i].layer, currentTime, fps);
        layersUI[i].layer.x = vals.x;
        layersUI[i].layer.y = vals.y;
        layersUI[i].layer.scale = vals.scale;
        layersUI[i].layer.rotation = vals.rotation;
        layersUI[i].layer.opacity = vals.opacity;
        if (layersUI[i].enableMotionBlur) {
            layersUI[i].imageDirty = true;
        }
    }
    updateCompositeImage();
    // Update motion paths if necessary
    updateMotionPaths();
    viewport()->update();
}

void PaintCanvas::createThreadWorker() {

    // Create new thread and worker for the stroke
    QThread* thread = new QThread(this);
    QString layerUuid = QUuid::createUuid().toString();
    Worker* worker = new Worker(layerUuid, activeLayerIndex, 0, strokeProperties, 0);
    worker->moveToThread(thread);
    m_threads.append(thread);
    m_currentWorker = worker;

    connect(this, &PaintCanvas::newPointAvailable, worker, &Worker::addPoint, Qt::QueuedConnection);
    connect(thread, &QThread::started, worker, &Worker::process);
    connect(worker, &Worker::resultReady, this, &PaintCanvas::handleResult);
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

// Implement mouse events
void PaintCanvas::mousePressEvent(QMouseEvent *event) {
    QPoint pos = event->pos();
    currentHandleContext = findHandleUnderCursor(pos);

    if (currentTool == ToolMode::Paint && this->dragMode() != QGraphicsView::ScrollHandDrag) {
        isDrawing = true;
        currentStrokePoints.clear();

        if(m_tempStrokeItem){
            m_scene->removeItem(m_tempStrokeItem);
            delete m_tempStrokeItem;
            m_tempStrokeItem = nullptr;
        }

        createThreadWorker();

        QPointF scenePos = mapToScene(pos);
        QPainterPath path;
        path.moveTo(pos);
        m_tempStrokeItem = new QGraphicsPathItem(path);

        currentStrokePoints.push_back({event->pos(), currentPressure});

        emit newPointAvailable(scenePos, currentPressure);

        //layersUI[activeLayerIndex].layerGroup->hide();

        // start worker etc.
        lastPos = pos;
        currentPressure = 0.5f; // if mouse
    } else if (currentTool == ToolMode::Select) {
        isSelecting = true;
        selectionRect.setTopLeft(mapToScene(pos));
        selectionRect.setBottomRight(mapToScene(pos));
        m_selectionItem->setRect(selectionRect);
        m_selectionItem->setVisible(true);
    } // etc for other tools
    QGraphicsView::mousePressEvent(event);
}

void PaintCanvas::mouseMoveEvent(QMouseEvent *event) {
    QPoint pos = event->pos();
    cursorPos = pos;
    m_cursorItem->setPos(mapToScene(pos));
    m_cursorItem->setVisible(showPaintCursor && currentTool == ToolMode::Paint);
    if (isDrawing) {
        // add point mapToScene(pos), pressure
        QPointF scenePos = mapToScene(pos);


        if(!currentStrokeGroup){
            currentStrokeGroup = new QGraphicsItemGroup;
            m_scene->addItem(currentStrokeGroup);
        }

        float widths = strokeProperties.maxWidth * currentPressure;
        QPen pen(Qt::black, widths, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        QPointF p0 = currentStrokePoints.end()->pos;
        QPointF p1 = mapToScene(lastPos);
        QPointF p2 = scenePos;
        //QGraphicsLineItem* line = new QGraphicsLineItem(p1.x(), p1.y(), p2.x(), p2.y());
        IndexedLineItem* line = new IndexedLineItem(p1.x(), p1.y(), p2.x(), p2.y(), currentStrokePoints.size());
        line->setPen(pen);
        currentStrokeGroup->addToGroup(line);

        currentStrokePoints.push_back({scenePos, currentPressure});
        emit newPointAvailable(scenePos, currentPressure);
        // worker will update via handleResult
    } else if (isSelecting) {
        selectionRect.setBottomRight(mapToScene(pos));
        m_selectionItem->setRect(selectionRect);
    } else if (currentHandleContext.isValid) {
        // handle drag, update positions, transforms
        // for example, for move, compute delta = mapToScene(pos) - mapToScene(lastPos)
        // update layer.x += delta.x() / m_zoomFactor
        // updateCompositeImage
    }

    lastPos = pos;
    QGraphicsView::mouseMoveEvent(event);

    viewport()->update();
}

void PaintCanvas::mouseReleaseEvent(QMouseEvent *event) {
    if (isDrawing) {
        isDrawing = false;
        if (m_currentWorker) m_currentWorker->requestCompletion();

        if(currentStrokeGroup){
            delete currentStrokeGroup;
            currentStrokeGroup = nullptr;
        }

    } else if (isSelecting) {
        isSelecting = false;
        // process selection, find strokes in rect
        m_selectionItem->setVisible(false);
    } // etc
    QGraphicsView::mouseReleaseEvent(event);
}

void PaintCanvas::tabletEvent(QTabletEvent *event) {

    cursorPos = event->posF().toPoint();
    currentPressure = event->pressure(); // [0.0, 1.0]


    if(m_verbose){
        GameFusion::Log().info() << "pressure "<<(float)currentPressure<<"\n";
        GameFusion::Log().info() << "Tablet Event: type=" << event->type()
                                 << ", pressure=" << (float)currentPressure
                                 << ", pos=" << cursorPos.x() << "," << cursorPos.y()
                                 << ", type=" << (int)event->type()
                                 << ", pointerType=" << (int)event->pointerType()
                                 << ", inProximity=" << (event->type() == QEvent::TabletEnterProximity ? "true" : "false")
                                 << "\n";
    }

    switch (event->type()) {
    case QEvent::TabletEnterProximity:
        isTabletActive = true;
        break;
    case QEvent::TabletLeaveProximity:
        isTabletActive = false;
        currentPressure = 0.0f;
        break;
    case QEvent::TabletPress:
        isTabletActive = true;
        mousePressEvent(new QMouseEvent(QEvent::MouseButtonPress, event->posF(), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        break;
    case QEvent::TabletMove:
        isTabletActive = true;
        if (currentPressure > 0) {

            mouseMoveEvent(new QMouseEvent(QEvent::MouseMove, event->posF(), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        } else {
            mouseMoveEvent(new QMouseEvent(QEvent::MouseMove, event->posF(), Qt::NoButton, Qt::NoButton, Qt::NoModifier));
        }
        break;
    case QEvent::TabletRelease:
        isTabletActive = true;
        mouseReleaseEvent(new QMouseEvent(QEvent::MouseButtonRelease, event->posF(), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
        isTabletActive = false;
        break;
    default:
        break;
    }
    event->accept();

    isTabletActive = false;
}



void PaintCanvas::wheelEvent(QWheelEvent *event) {
    //if (event->modifiers() & Qt::ControlModifier)
    if(true){
        if (event->angleDelta().y() > 0) {
            zoomIn();
        } else {
            zoomOut();
        }
        event->accept();
    } else {
        QGraphicsView::wheelEvent(event);
    }
}

// Implementation for enterEvent
void PaintCanvas::enterEvent(QEnterEvent *event) {
    // Handle mouse/stylus entry (original comment)
    // Example: Set cursor or track entry
    showPaintCursor = true;  // Or your logic
    QGraphicsView::enterEvent(event);  // Call base
}

// Implementation for focusInEvent
void PaintCanvas::focusInEvent(QFocusEvent *event) {
    // Optional: Debug focus gain (original comment)
    qDebug() << "PaintCanvas gained focus";
    QGraphicsView::focusInEvent(event);  // Call base
}

// Implementation for focusOutEvent
void PaintCanvas::focusOutEvent(QFocusEvent *event) {
    // Optional: Debug focus loss (original comment)
    qDebug() << "PaintCanvas lost focus";
    QGraphicsView::focusOutEvent(event);  // Call base
}

// Implementation for keyPressEvent
void PaintCanvas::keyPressEvent(QKeyEvent *event) {
    // Original implementation (from provided paintarea.cpp snippet)
    // Add your key handling here, e.g.:
    if (event->key() == Qt::Key_Space) {
        // Example: Toggle something
        setDragMode(QGraphicsView::ScrollHandDrag);
        setCursor(Qt::OpenHandCursor);
    } else if (event->key() == Qt::Key_Plus) {
        zoomIn();
    } else if (event->key() == Qt::Key_Minus) {
        zoomOut();
    } else if (event->key() == Qt::Key_F) {
        fitToBase();
    } else if (event->key() == Qt::Key_0 && event->modifiers() == Qt::ControlModifier) {
        resetZoom();
    }

    QGraphicsView::keyPressEvent(event);  // Call base to propagate
}

void PaintCanvas::keyReleaseEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Space) {
        setDragMode(QGraphicsView::NoDrag); // Or restore previous mode if tracked
        unsetCursor(); // Restore default cursor
    }
    QGraphicsView::keyReleaseEvent(event);
}

// Implementation for handleThreadFinished (slot)
void PaintCanvas::handleThreadFinished(QThread* thread) {
    // Original implementation
    int threadIndex = m_threads.indexOf(thread);
    m_threads.removeOne(thread);
    if (m_verbose) {
        GameFusion::Log().print() << "Finishing thread " << threadIndex << "\n";
    }
}

// Implementation for findHandleUnderCursor (private)
HandleContext PaintCanvas::findHandleUnderCursor(const QPoint &pos) {
    // Implement logic to find handle under cursor
    // Original likely checked camera/layer/selection handles
    // Example stub (adapt to your HandleContext logic):
    HandleContext context;
    QPointF scenePos = mapToScene(pos);

    // Check camera handles
    for (int i = 0; i < m_cameraHandleGroups.size(); ++i) {
        QList<QGraphicsItem*> handles = m_cameraHandleGroups[i]->childItems();
        for (int j = 0; j < handles.size(); ++j) {
            if (handles[j]->contains(handles[j]->mapFromScene(scenePos))) {
                context.isValid = true;
                context.frameType = HandleContext::Camera;
                context.frameIndex = i;
                context.handleIndex = j;
                // Set handleType based on j (e.g., 0=ResizeTL, etc.)
                return context;
            }
        }
    }

    // Similarly check layer motion handles, bezier controls, etc.
    // For layers:
    // ...

    context.isValid = false;
    return context;
}

// Implementation for computeLayerValuesAtTime (private)
LayerValues PaintCanvas::computeLayerValuesAtTime(const GameFusion::Layer& layer, double timeMs, float fps) {
    // Original logic: Interpolate layer values from keyframes
    // Use helper from ScriptBreakdown.h or implement linear interp
    LayerValues vals;

    // Example for position (adapt for scale, rotation, opacity)
    if (!layer.motionKeyframes.empty()) {
        // Find prev/next keyframes
        GameFusion::Layer::MotionKeyFrame prev = layer.motionKeyframes[0];
        GameFusion::Layer::MotionKeyFrame next = layer.motionKeyframes[0];
        double currentTime = timeMs / (1000.0 / fps);  // Convert ms to frames if needed

        for (const auto& kf : layer.motionKeyframes) {
            if (kf.time <= currentTime) prev = kf;
            if (kf.time >= currentTime && kf.time < next.time) next = kf;
        }

        if (currentTime <= prev.time) {
            vals.x = prev.x;
            vals.y = prev.y;
        } else if (currentTime >= next.time) {
            vals.x = next.x;
            vals.y = next.y;
        } else {
            double t = (currentTime - prev.time) / (next.time - prev.time);
            vals.x = prev.x + t * (next.x - prev.x);
            vals.y = prev.y + t * (next.y - prev.y);
        }
    }

    // Similarly for opacityKeyframes, etc.
    // vals.scale = ...
    // vals.rotation = ...
    // vals.opacity = ...

    return vals;
}

void PaintCanvas::setStrokeProperties(const StrokeProperties &props)
{
    strokeProperties = props;

    // Ensure minWidth does not exceed maxWidth
    if (strokeProperties.minWidth > strokeProperties.maxWidth) {
        strokeProperties.minWidth = strokeProperties.maxWidth;
    }

    for (const auto& selStroke : currentSelection.selectedStrokes) {
        selStroke.stroke->setStrokeProperties(props);
        layersUI[selStroke.layerIndex].imageDirty =  true;
    }

    isEditing = true;
    updateCompositeImage();
    update(); // Trigger repaint
    isEditing = false;
}

void PaintCanvas::fitToBase() {
    fitInView(QRectF(0, 0, m_baseSize.width(), m_baseSize.height()), Qt::KeepAspectRatio);
    m_zoomFactor = transform().m11();  // Update zoom factor based on current scale
    viewport()->update();
}

void PaintCanvas::addStrokeToScene(const std::vector<GameFusion::Vector3D> &stroke) {
    // Create path item (or rasterize to pixmap)
    //QGraphicsPathItem *strokeItem = new QGraphicsPathItem(path);
    QPen pen(strokeProperties.backgroundColor, strokeProperties.maxWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    //strokeItem->setPen(pen);

    // Add to your active LayerGroupItem
    LayerGroupItem *activeGroup = m_layerItems[activeLayerIndex];

    StrokeGroupItem *strokeGroup = new StrokeGroupItem;

    for(int i=1; i<stroke.size(); i++){
        const GameFusion::Vector3D &v1 = stroke[i-1];
        const GameFusion::Vector3D &v2 = stroke[i];

        QGraphicsLineItem *line = new QGraphicsLineItem(
            v1.x(), v1.y(),
            v2.x(), v2.y()
            );
        line->setPen(pen);
        strokeGroup->addToGroup(line);
    }

    activeGroup->addToGroup(strokeGroup);
    activeGroup->setUseRasterizedImage(true); // Using your existing method\

    //layersUI[activeLayerIndex].imageDirty = true;
    //updateCompositeImage();
    update(); // Trigger repaint

}
