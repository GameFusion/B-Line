#include "PaintCanvas.h"
#include "PlaybackTiming.h"
#include <QApplication>
#include <QAction>
#include <QDir>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QThread>
#include <QToolBar>
#include <QTimer>
#include <QTabletEvent>
#include <cstdio>
#include <cstdlib>

static int checks = 0;
static void require(bool result, const char *message) {
    ++checks;
    if (!result) { fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static GameFusion::BezierCurve line(float x1, float y1, float x2, float y2, QColor color) {
    GameFusion::BezierCurve c;
    c += GameFusion::BezierControl({x1,y1,0},{0,0,0},{0,0,0});
    c += GameFusion::BezierControl({x2,y2,0},{0,0,0},{0,0,0});
    StrokeProperties p; p.foregroundColor=color; p.maxWidth=8; p.minWidth=2; p.smoothness=0.5;
    c.setStrokeProperties(p); c.assess(20,false); return c;
}
static QImage pictureImage(PaintArea &area) {
    QPicture picture=area.workspacePicture();
    QImage image(900,600,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::white);
    QPainter p(&image); p.translate(160,80);
    p.scale(qreal(picture.logicalDpiX())/image.logicalDpiX(),qreal(picture.logicalDpiY())/image.logicalDpiY());
    p.drawPicture(QPointF(),picture); p.end(); return image;
}
static QColor pixel(const QImage &image, int x, int y) { return image.pixelColor(x+160,y+80); }
static void sendMouse(PaintCanvas &view,QEvent::Type type,QPointF scene,Qt::MouseButton button,Qt::MouseButtons buttons) {
    QPointF pos=view.mapFromScene(scene);
    QMouseEvent event(type,pos,view.viewport()->mapToGlobal(pos.toPoint()),button,buttons,Qt::NoModifier);
    QCoreApplication::sendEvent(view.viewport(),&event);
}
static void lightTableChecks(QApplication &app) {
    QTemporaryDir assets;
    QDir().mkpath(assets.filePath("movies"));
    QImage reference(320,240,QImage::Format_ARGB32_Premultiplied);
    reference.fill(QColor(80,100,120)); reference.save(assets.filePath("movies/background.png"));
    QImage patch(320,240,QImage::Format_ARGB32_Premultiplied); patch.fill(Qt::transparent);
    {QPainter p(&patch);p.fillRect(QRect(240,160,60,60),QColor(130,40,180));}
    patch.save(assets.filePath("patch.png"));
    GameFusion::Panel panel;panel.uuid="light-table";panel.image="background.png";
    GameFusion::Layer active;active.uuid="active";active.x=20;
    active.strokes.push_back(line(-100,60,270,60,Qt::red));
    GameFusion::Layer upper;upper.uuid="upper";
    upper.strokes.push_back(line(150,110,150,210,Qt::blue));
    upper.blendMode=GameFusion::BlendMode::Multiply;
    GameFusion::Layer lower;lower.uuid="lower";
    lower.strokes.push_back(line(-100,150,290,150,Qt::green));
    lower.imageFilePath=assets.filePath("patch.png").toStdString();
    panel.layers={active,upper,lower};
    PaintArea area;area.setDimensions(320,240,320,240);
    area.toggleOutputFrame(false);area.toggleActionSafe(false);area.toggleTitleSafe(false);area.setPipDisplay(false);
    area.setProjectPath(assets.path());area.setPanel(panel);area.setActiveLayer("active");
    // Compare scene pixels independently of the physical mouse's brush cursor.
    area.setToolMode(PaintArea::ToolMode::Select);
    PaintCanvas view;view.resize(1000,650);view.setPaintArea(&area);view.show();
    app.processEvents();view.fitToBase();
    auto *toolbar=view.findChild<QToolBar*>("workspaceToolbar");
    auto *light=view.findChild<QAction*>("workspaceLightTable");
    require(toolbar && light,"workspace has named toolbar and light-table control");
    int icons=0;for(auto *action:toolbar->actions())if(!action->icon().isNull())++icons;
    require(icons==13,"Font Awesome icons cover tools, navigation and preview toggles");
    QAction undo("Undo stroke",&view),redo("Redo stroke",&view);undo.setEnabled(false);
    view.setHistoryActions(&undo,&redo);
    QAction *undoButton=nullptr;for(auto *action:toolbar->actions())if(action->text()==undo.text())undoButton=action;
    require(undoButton && !undoButton->icon().isNull() && !undoButton->isEnabled(),"icon Undo retains shared enabled state");
    bool undone=false;QObject::connect(&undo,&QAction::triggered,&view,[&]{undone=true;});
    undo.setEnabled(true);undoButton->trigger();require(undone,"workspace Undo invokes original history action");
    QImage normal=pictureImage(area),exportBefore(320,240,QImage::Format_ARGB32_Premultiplied);
    area.renderFrameToImage(exportBefore);
    area.setLightTableMode(true);
    require(light->isChecked(),"source light-table change checks workspace toggle");
    PaintCanvas second;second.setPaintArea(&area);
    require(second.findChild<QAction*>("workspaceLightTable")->isChecked(),"new workspace inherits current light-table state");
    QImage faded=pictureImage(area);
    auto expectedFade=[](QColor c){return QColor(qRound(255*0.75+c.red()*0.25),qRound(255*0.75+c.green()*0.25),qRound(255*0.75+c.blue()*0.25));};
    auto near=[](QColor a,QColor b){return qAbs(a.red()-b.red())<=2&&qAbs(a.green()-b.green())<=2&&qAbs(a.blue()-b.blue())<=2;};
    require(near(pixel(faded,200,200),expectedFade(pixel(normal,200,200))),"panel reference image fades to 25 percent");
    require(near(pixel(faded,260,180),expectedFade(pixel(normal,260,180))),"image layer fades with background");
    require(near(pixel(faded,150,150),expectedFade(pixel(normal,150,150))),"background overlap and multiply blend fade once as a group");
    require(near(pixel(faded,-70,150),expectedFade(pixel(normal,-70,150))),"light table preserves off-canvas background strokes");
    require(pixel(faded,-70,60).red()>240 && pixel(faded,-70,60).green()<10,"active off-canvas ink stays full strength");
    QImage integrated(320,240,QImage::Format_ARGB32_Premultiplied);integrated.fill(Qt::white);
    {QPainter p(&integrated);area.renderScene(p);}
    require(near(integrated.pixelColor(150,150),pixel(faded,150,150)) &&
            near(integrated.pixelColor(200,200),pixel(faded,200,200)),"integrated and workspace light table agree");
    QImage exportAfter(320,240,QImage::Format_ARGB32_Premultiplied);area.renderFrameToImage(exportAfter);
    require(exportBefore==exportAfter && area.compositedImage().pixelColor(140,60).green()<10,
            "light table leaves active ink in export and camera source composite");
    area.setLayerVisibility("active",false);
    require(pixel(pictureImage(area),140,60).green()>200,"hidden active layer remains hidden in light table");
    area.setLayerVisibility("active",true);active.opacity=0.5;area.updateLayer(active);
    require(pixel(pictureImage(area),140,60).green()>90 && pixel(pictureImage(area),140,60).green()<130,
            "active layer retains its authored opacity");
    area.setActiveLayer("lower");
    require(pixel(pictureImage(area),100,150).green()>240 && pixel(pictureImage(area),100,150).red()<10,
            "changing active layer moves full-strength highlight");
    area.setActiveLayer("active");active.opacity=1;area.updateLayer(active);
    app.processEvents();view.grab().save("/tmp/boarder-workspace-lighttable.png");
    light->trigger();require(!area.lightTableMode(),"workspace toggle updates shared light-table state");
    require(pictureImage(area)==normal,"turning light table off restores the full scene");
    GameFusion::Panel next;next.uuid="next-light-table";active.uuid="next-active";active.opacity=0.5;
    next.layers={active};area.setLightTableMode(true);area.setPanel(next);
    require(qAbs(pixel(pictureImage(area),140,60).green()-128)<=2,
            "panel switch excludes the default active layer from faded background");
}

int main(int argc,char **argv) {
    QApplication app(argc,argv);
    lightTableChecks(app);
    PaintArea area;
    area.setDimensions(320,240,320,240);
    area.toggleOutputFrame(false); area.toggleActionSafe(false); area.toggleTitleSafe(false); area.setPipDisplay(false);
    GameFusion::Panel panel; panel.uuid="workspace-test";
    GameFusion::Layer ink; ink.uuid="ink"; ink.name="Ink";
    ink.strokes.push_back(line(-100,80,480,80,Qt::red));
    panel.layers.push_back(ink);
    area.setPanel(panel); area.setActiveLayer("ink");
    PaintCanvas view; view.resize(1000,650); view.setPaintArea(&area); view.show();
    app.processEvents(); view.fitToBase();
    require(!view.viewport()->inherits("QOpenGLWidget"),"ordinary QWidget viewport");
    QImage image=pictureImage(area);
    image.save("/tmp/boarder-workspace-render.png");
    require(pixel(image,140,80).red()>220 && pixel(image,140,80).green()<60,"stroke inside output");
    require(pixel(image,-70,80).red()>220 && pixel(image,-70,80).green()<60,"negative-space stroke survives");
    require(pixel(image,430,80).green()<60,"stroke beyond canvas survives");
    require(pixel(image,140,140)==QColor(Qt::white),"no diagnostic diagonal");
    QImage integrated(320,240,QImage::Format_ARGB32_Premultiplied); integrated.fill(Qt::white);
    {QPainter p(&integrated); area.renderScene(p);}
    require(integrated.pixelColor(140,80)==pixel(image,140,80),"integrated and workspace color agree");
    auto hidden=ink; hidden.visible=false; area.updateLayer(hidden);
    require(pixel(pictureImage(area),140,80)==QColor(Qt::white),"layer visibility updates both views");
    ink.opacity=0.5; area.updateLayer(ink); image=pictureImage(area);
    require(pixel(image,140,80).green()>100 && pixel(image,140,80).green()<160,"layer opacity retained in cached picture");
    ink.strokes.push_back(line(140,30,140,160,Qt::red)); area.updateLayer(ink);
    image=pictureImage(area);
    require(qAbs(pixel(image,140,80).green()-pixel(image,100,80).green())<5,
            "overlapping strokes receive layer opacity once");
    ink.strokes.pop_back();
    ink.opacity=1; ink.x=0; ink.y=40; area.updateLayer(ink); image=pictureImage(area);
    require(pixel(image,140,120).green()<60 && pixel(image,140,80)==QColor(Qt::white),"layer transform moves content");
    ink.y=0; area.updateLayer(ink);
    const QSize original=area.size(); const double sourceZoom=area.zoomFactor();
    view.applyZoom(2.0); view.resize(900,620); app.processEvents();
    require(area.size()==original && area.zoomFactor()==sourceZoom,"workspace zoom/resize never resizes document");
    require(qAbs(view.zoomFactor()-2)<0.001,"resize preserves workspace zoom");
    view.centerOn(120,100);
    area.applyZoom(0.5); app.processEvents();
    const QPointF source=view.sourcePosition(view.mapFromScene(QPointF(120,100)));
    require(QLineF(source,QPointF(60,50)).length()<1,"input accounts for independent zoom and pan");
    require(qAbs(view.zoomFactor()-2)<0.001,"integrated zoom leaves workspace zoom unchanged");
    int modifications=0; GameFusion::Layer changed;
    QObject::connect(&area,&PaintArea::layerModified,&area,[&](const GameFusion::Layer &layer){++modifications;changed=layer;});
    area.setToolMode(PaintArea::ToolMode::Paint);
    sendMouse(view,QEvent::MouseButtonPress,{30,150},Qt::LeftButton,Qt::LeftButton);
    for(int i=1;i<=12;++i){sendMouse(view,QEvent::MouseMove,{30.0+i*8,150.0+i},Qt::NoButton,Qt::LeftButton);app.processEvents();}
    sendMouse(view,QEvent::MouseButtonRelease,{126,162},Qt::LeftButton,Qt::NoButton);
    QElapsedTimer wait; wait.start();
    while(modifications==0 && wait.elapsed()<5000){app.processEvents();QThread::msleep(5);}
    require(modifications>0 && changed.strokes.size()==2,"standalone drawing updates shared document");
    const auto &first=changed.strokes.back()[0].point;
    require(qAbs(first.x()-30)<3 && qAbs(first.y()-150)<3,"stroke coordinates independent of view transforms");
    area.setToolMode(PaintArea::ToolMode::Select);
    sendMouse(view,QEvent::MouseButtonPress,{15,140},Qt::LeftButton,Qt::LeftButton);
    sendMouse(view,QEvent::MouseMove,{145,180},Qt::NoButton,Qt::LeftButton);
    sendMouse(view,QEvent::MouseButtonRelease,{145,180},Qt::LeftButton,Qt::NoButton);
    QKeyEvent erase(QEvent::KeyPress,Qt::Key_Delete,Qt::NoModifier);
    QCoreApplication::sendEvent(&view,&erase);
    require(changed.strokes.size()==1,"selection and deletion use existing controller");
    const int count=modifications;
    const int h=view.horizontalScrollBar()->value();
    sendMouse(view,QEvent::MouseButtonPress,{100,100},Qt::MiddleButton,Qt::MiddleButton);
    sendMouse(view,QEvent::MouseMove,{125,100},Qt::NoButton,Qt::MiddleButton);
    sendMouse(view,QEvent::MouseButtonRelease,{125,100},Qt::MiddleButton,Qt::NoButton);
    require(view.horizontalScrollBar()->value()!=h && modifications==count,"middle drag pans without drawing");
    area.applyZoom(2); area.setToolMode(PaintArea::ToolMode::Paint);
    view.fitToBase();app.processEvents();view.grab().save("/tmp/boarder-workspace-window.png");
    GameFusion::Panel empty;empty.uuid="empty";area.setPanel(empty);app.processEvents();
    require(pixel(pictureImage(area),140,80)==QColor(Qt::white),"panel switch clears cached drawing");
    sendMouse(view,QEvent::MouseButtonPress,{40,40},Qt::LeftButton,Qt::LeftButton);
    sendMouse(view,QEvent::MouseButtonRelease,{40,40},Qt::LeftButton,Qt::NoButton);
    require(modifications==count,"empty panel input is safe");
    // Image, text, pressure/taper/gradient strokes, animated layers and camera guides.
    QTemporaryDir assets;
    QImage tile(64,48,QImage::Format_ARGB32_Premultiplied);tile.fill(QColor(30,160,220));
    const QString imagePath=assets.filePath("reference.png");tile.save(imagePath);
    GameFusion::Panel rich;rich.uuid="rich";
    GameFusion::Layer foreground;foreground.uuid="foreground";
    foreground.strokes.push_back(line(20,70,290,70,Qt::darkGreen));
    StrokeProperties pressure;pressure.maxWidth=20;pressure.minWidth=2;
    pressure.variableWidthMode=StrokeProperties::TaperOut;pressure.colorMode=StrokeProperties::GradientFGtoBG;
    pressure.foregroundColor=Qt::blue;pressure.backgroundColor=Qt::red;
    auto curve=line(20,115,290,115,Qt::blue);curve.setStrokeProperties(pressure);foreground.strokes.push_back(curve);
    GameFusion::Layer::TextContent label;label.text="Shared drawing workspace";label.fontName="Arial";
    label.fontSize=18;label.color="#ff101010";label.x=20;label.y=45;foreground.textContents.push_back(label);
    GameFusion::Layer::MotionKeyFrame a,b;a.time=0;b.time=25;b.x=40;
    foreground.motionKeyframes={a,b};
    GameFusion::Layer background;background.uuid="background";background.imageFilePath=imagePath.toStdString();
    rich.layers={foreground,background};
    GameFusion::CameraAnimation cameras;GameFusion::CameraFrame camera;camera.panelUuid=rich.uuid;
    camera.name="Opening";camera.x=0;camera.y=0;camera.zoom=0.8;cameras.frames.push_back(camera);
    area.setPanel(rich,0,25,cameras);area.setActiveLayer("foreground");
    area.setCurrentTime(0); image=pictureImage(area);
    require(pixel(image,220,180).blue()>180 && pixel(image,220,180).red()<70,"image layer renders");
    require(pixel(image,140,70).green()<170 && pixel(image,140,70).blue()<100,"foreground order over image");
    int darkText=0;for(int y=20;y<47;++y)for(int x=20;x<260;++x){auto c=pixel(image,x,y);if(c.red()<60&&c.green()<60&&c.blue()<60)++darkText;}
    require(darkText>50,"text uses shared font rendering");
    foreground.blendMode=GameFusion::BlendMode::Multiply;
    foreground.strokes[0]=line(20,70,290,70,Qt::red);area.updateLayer(foreground);
    const QColor multiplied=pixel(pictureImage(area),140,70);
    require(multiplied.red()<60 && multiplied.green()<20 && multiplied.blue()<20,"layer multiply blends with lower image");
    foreground.blendMode=GameFusion::BlendMode::Opacity;area.updateLayer(foreground);
    area.setCurrentTime(1000); area.update();
    require(pictureImage(area)!=image,"timeline animation changes workspace");
    area.setCurrentTime(0);area.setPipDisplay(true);area.setToolMode(PaintArea::ToolMode::Camera);
    area.toggleOutputFrame(true);area.toggleActionSafe(true);area.toggleTitleSafe(true);
    view.fitToBase();app.processEvents();view.grab().save("/tmp/boarder-workspace-rich-window.png");
    require(area.hasPipImage(),"camera preview available through shared renderer");
    int cameraChanges=0;QObject::connect(&area,&PaintArea::cameraFrameUpdated,&area,[&](const GameFusion::CameraFrame &,bool){++cameraChanges;});
    // Camera move handle uses the exact same coordinates and controller in both views.
    sendMouse(view,QEvent::MouseButtonPress,{128,96},Qt::LeftButton,Qt::LeftButton);
    sendMouse(view,QEvent::MouseMove,{145,105},Qt::NoButton,Qt::LeftButton);
    sendMouse(view,QEvent::MouseButtonRelease,{145,105},Qt::LeftButton,Qt::NoButton);
    require(cameraChanges>0,"camera interaction updates shared model");
    area.setPipDisplay(false);area.setToolMode(PaintArea::ToolMode::Paint);
    const int beforeTablet=modifications;
    auto tabletEvent=[&](QEvent::Type type,QPointF scene,qreal pressureValue,Qt::MouseButton button,Qt::MouseButtons buttons){
        QPointF pos=view.mapFromScene(scene);
        QTabletEvent e(type,QPointingDevice::primaryPointingDevice(),pos,view.viewport()->mapToGlobal(pos.toPoint()),
                       pressureValue,0,0,0,0,0,Qt::NoModifier,button,buttons);
        QCoreApplication::sendEvent(view.viewport(),&e);
    };
    tabletEvent(QEvent::TabletPress,{30,190},0.2,Qt::LeftButton,Qt::LeftButton);
    for(int i=1;i<=12;++i)tabletEvent(QEvent::TabletMove,{30.0+i*8,190.0+i},0.2+i*0.05,Qt::NoButton,Qt::LeftButton);
    tabletEvent(QEvent::TabletRelease,{126,202},0,Qt::LeftButton,Qt::NoButton);
    wait.restart();while(modifications==beforeTablet && wait.elapsed()<5000){app.processEvents();QThread::msleep(5);}
    require(modifications>beforeTablet,"tablet drawing reaches shared stroke worker");
    view.hide();view.show();app.processEvents();
    GameFusion::Panel dense;dense.uuid="dense";GameFusion::Layer denseLayer;denseLayer.uuid="dense-ink";
    for(int i=0;i<300;++i)denseLayer.strokes.push_back(line(-50,i*2,370,i*2,QColor::fromHsv(i%360,180,180)));
    dense.layers.push_back(denseLayer);area.setPanel(dense);area.setActiveLayer("dense-ink");
    view.fitToBase();app.processEvents();view.grab();
    QElapsedTimer performance;performance.start();
    for(int i=0;i<20;++i)view.grab();
    printf("Cached workspace redraw, 300 strokes: %.2f ms/frame\n",performance.elapsed()/20.0);
    // Inactive views and camera previews stay frozen until release, from either origin.
    area.setPlaybackDisplay("Playing  01:00:00:12");
    area.setPipDisplay(true); area.show(); app.processEvents(); view.grab();
    int workspaceUpdates = 0, thumbnails = 0;
    QObject::connect(&area, &PaintArea::workspaceChanged, &area, [&] { ++workspaceUpdates; });
    QObject::connect(&area, &PaintArea::compositImageModified, &area, [&] { ++thumbnails; });
    auto legacyMouse = [&](QEvent::Type type, QPointF pos, Qt::MouseButton button, Qt::MouseButtons buttons) {
        QMouseEvent e(type, pos, area.mapToGlobal(pos.toPoint()), button, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(&area, &e);
    };
    area.setPipDisplay(false); // Keep legacy input clear of the draggable preview.
    workspaceUpdates = 0; thumbnails = 0;
    legacyMouse(QEvent::MouseButtonPress, {40,40}, Qt::LeftButton, Qt::LeftButton);
    for (int i=1;i<=15;++i) {
        legacyMouse(QEvent::MouseMove, {40.0+i*5,40.0+i}, Qt::NoButton, Qt::LeftButton);
        app.processEvents(); QThread::msleep(2);
    }
    require(area.interactionActive() && !area.workspaceInteractionActive(), "legacy owns held stroke");
    require(workspaceUpdates == 0 && thumbnails == 0, "legacy stroke defers workspace and thumbnails");
    const int priorLegacy = modifications;
    legacyMouse(QEvent::MouseButtonRelease, {115,55}, Qt::LeftButton, Qt::NoButton);
    wait.restart(); while(modifications==priorLegacy && wait.elapsed()<5000) {app.processEvents();QThread::msleep(5);}
    require(!area.interactionActive() && workspaceUpdates>0 && thumbnails>0, "legacy release refreshes secondary views");
    area.setPipDisplay(true); view.grab();
    const QImage cameraBefore = area.currentPipImage();
    thumbnails=0;
    sendMouse(view,QEvent::MouseButtonPress,{40,170},Qt::LeftButton,Qt::LeftButton);
    for(int i=1;i<=12;++i)sendMouse(view,QEvent::MouseMove,{40.0+i*8,170.0+i},Qt::NoButton,Qt::LeftButton);
    view.grab();
    require(area.workspaceInteractionActive(), "workspace owns held stroke");
    require(cameraBefore == area.currentPipImage() && thumbnails==0, "camera and thumbnails stay frozen while drawing");
    const int beforeRapid = modifications;
    sendMouse(view,QEvent::MouseButtonRelease,{136,182},Qt::LeftButton,Qt::NoButton);
    sendMouse(view,QEvent::MouseButtonPress,{40,200},Qt::LeftButton,Qt::LeftButton);
    for(int i=1;i<=12;++i)sendMouse(view,QEvent::MouseMove,{40.0+i*8,200.0+i},Qt::NoButton,Qt::LeftButton);
    sendMouse(view,QEvent::MouseButtonRelease,{136,212},Qt::LeftButton,Qt::NoButton);
    wait.restart(); while(modifications<beforeRapid+2 && wait.elapsed()<5000) {app.processEvents();QThread::msleep(5);}
    require(modifications==beforeRapid+2, "rapid consecutive strokes both finish exactly once");
    require(changed.strokes.size()==303, "rapid strokes preserve original layer and earlier ink");
    area.setPlaybackMode(true); thumbnails=0;
    area.updateCompositeImage();
    require(thumbnails==0, "playback does not rebuild timeline thumbnails");
    area.setPlaybackMode(false);
    require(thumbnails>0, "pause flushes timeline thumbnail");
    view.grab().save("/tmp/boarder-playback-workspace.png");
    QImage exportBefore(320,240,QImage::Format_ARGB32_Premultiplied), exportAfter(320,240,QImage::Format_ARGB32_Premultiplied);
    area.renderFrameToImage(exportBefore);
    area.setPlaybackDisplay("Paused  12:34:56:20");
    area.renderFrameToImage(exportAfter);
    require(!exportBefore.isNull() && exportBefore == exportAfter, "timecode overlay never burns into exported frame");
    require(PlaybackTiming::timecode(1040,25,"01:00:00:00")=="01:00:01:01", "sequence offset and project fps timecode");
    require(PlaybackTiming::timecode(60000,24)=="00:01:00:00", "timecode rolls across minute");
    require(PlaybackTiming::timecode(33,30)=="00:00:00:01", "rounded millisecond cursor retains frame identity");
    require(PlaybackTiming::lastFrame(1000,24)==958, "natural end holds last valid frame");
    require(PlaybackTiming::frameTime(100,25)==80, "elapsed clock skips to current frame without accumulating drift");
    int offscreenStrokes=0;
    QObject::connect(&area,&PaintArea::strokeCompleted,&area,[&](const QString &panelId,const QString &layerId,const GameFusion::BezierCurve &curve){
        require(panelId=="dense" && layerId=="dense-ink" && !curve.empty(),"late fit retains original panel and layer identity");++offscreenStrokes;
    });
    sendMouse(view,QEvent::MouseButtonPress,{20,40},Qt::LeftButton,Qt::LeftButton);
    sendMouse(view,QEvent::MouseMove,{95,60},Qt::NoButton,Qt::LeftButton);
    sendMouse(view,QEvent::MouseButtonRelease,{95,60},Qt::LeftButton,Qt::NoButton);
    GameFusion::Panel next;next.uuid="next";GameFusion::Layer nextLayer;nextLayer.uuid="next-ink";next.layers.push_back(nextLayer);
    area.setPanel(next);area.setActiveLayer("next-ink");area.finishPendingStrokes();
    require(offscreenStrokes==1,"panel navigation delivers pending ink to original model");
    sendMouse(view,QEvent::MouseButtonPress,{30,50},Qt::LeftButton,Qt::LeftButton);
    sendMouse(view,QEvent::MouseMove,{100,70},Qt::NoButton,Qt::LeftButton);
    QFocusEvent focusOut(QEvent::FocusOut);QCoreApplication::sendEvent(&view,&focusOut);
    area.finishPendingStrokes();
    require(!area.interactionActive(),"lost workspace focus finishes held stroke and unfreezes views");
    view.setPaintArea(nullptr);view.grab();
    require(view.paintArea()==nullptr,"source detachment is safe");
    wait.restart();while(wait.elapsed()<250){app.processEvents();QThread::msleep(5);}
    printf("PaintCanvasSmoke: PASS (%d checks)\n",checks);
    return 0;
}
