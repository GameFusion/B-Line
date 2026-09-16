### Recommendations for Implementing Canvas Size, Margins, and Adaptable Drawing Area

Based on your description, I'll outline a structured approach to add support for a default canvas size (larger drawing area with margins around the output resolution frame), safe frames, and per-shot overrides. This aligns with best practices in storyboarding/animation software (e.g., Toon Boom, Storyboard Pro):

- **Key Concepts**:
  - **Output Resolution**: The final render size (e.g., 1920x1080 from `resolutionComboBox`). This is the "frame" at the center.
  - **Canvas Size**: A larger fixed area for drawing, with the output frame centered. This provides "overscan" or "extra margins" for camera pans, zooms, or overflow elements (your "provisioned space for extra margins").
  - **Safe Frame**: As you noted, this is typically a percentage (e.g., 90%) within the **output resolution** for safe action/title areas (not the canvas). Keep it as-is for best practice—it's not the same as canvas margins.
  - **Best Practice**: In production, canvas is often 20-50% larger than resolution on each side to allow for flexibility without resizing mid-project. Store defaults at project level, allow per-shot overrides for custom needs (e.g., a wide pan shot needs more horizontal margin).
  - **Storage**: Project-level in `project.json`. Shot-level overrides in `Shot` struct (optional; fall back to project if unset).
  - **PaintArea Adaptation**: When setting a panel (via `setPanel`), dynamically resize `PaintArea`'s `compositeImage`, `m_baseSize`, and layers based on the shot's canvas size (or project default). Draw the output frame as a centered rectangle with guides.

This ensures the drawing area "adapts" per shot without losing data (resize images non-destructively).

#### 1. Extend `NewProjectDialog` for Canvas Margins
Add a new field for "Canvas Overscan Margin (%)" to define extra space around the output resolution. This computes canvas size as:
- Canvas Width = Resolution Width + (Resolution Width * Margin % / 100)  // Extra on left + right (symmetric).
- Same for Height. (Preserves aspect ratio implicitly since margin % is uniform.)

**Changes to `NewProjectDialog.ui`** (Add to the QFormLayout after "Safe Frame"):
```xml
<item row="4" column="1">  <!-- After safeFrameComboBox -->
  <widget class="QComboBox" name="canvasMarginComboBox">
    <item><property name="text"><string>0% (No Margin)</string></property></item>
    <item><property name="text"><string>10%</string></property></item>
    <item><property name="text"><string>20% (Recommended)</string></property></item>
    <item><property name="text"><string>30%</string></property></item>
    <item><property name="text"><string>50%</string></property></item>
    <item><property name="text"><string>Custom</string></property></item>
  </widget>
</item>
<item row="4" column="0">
  <widget class="QLabel" name="labelCanvasMargin">
    <property name="text">
      <string>Canvas Overscan Margin (%)</string>
    </property>
  </widget>
</item>
<!-- For Custom: Add a hidden LineEdit, shown via signal-slot if "Custom" selected -->
<item row="4" column="2">  <!-- Optional: Next to ComboBox -->
  <widget class="QLineEdit" name="canvasMarginCustomEdit">
    <property name="visible"><bool>false</bool></property>  <!-- Hidden by default -->
    <property name="placeholderText"><string>Enter % (e.g., 15)</string></property>
  </widget>
</item>
```

**Changes to `NewProjectDialog.h`**:
```cpp
QString canvasMargin() const;  // New getter
```

**Changes to `NewProjectDialog.cpp`**:
- In constructor: Connect ComboBox `currentTextChanged` to a slot that shows/hides `canvasMarginCustomEdit` if "Custom".
```cpp
connect(ui->canvasMarginComboBox, &QComboBox::currentTextChanged, this, [this](const QString& text) {
    bool isCustom = (text == "Custom");
    ui->canvasMarginCustomEdit->setVisible(isCustom);
    if (isCustom) ui->canvasMarginCustomEdit->setFocus();
});
```
- New getter:
```cpp
QString NewProjectDialog::canvasMargin() const {
    QString selected = ui->canvasMarginComboBox->currentText();
    if (selected == "Custom") return ui->canvasMarginCustomEdit->text();
    return selected.left(selected.indexOf('%'));  // e.g., "20%" -> "20"
}
```

#### 2. Extend Project JSON and `newProject()`
In `newProject()`, compute canvas size based on margin % and store in `projectJson`.

**Changes to `MainWindow::newProject()`** (after parsing resolution):
```cpp
int marginPct = dialog.canvasMargin().toInt();  // 0 if empty/invalid
int marginW = resWidth * marginPct / 100;  // Total extra (split left/right)
int marginH = resHeight * marginPct / 100;
int canvasWidth = resWidth + marginW;
int canvasHeight = resHeight + marginH;

projectJson["canvas"] = QJsonArray{canvasWidth, canvasHeight};
projectJson["canvas_margin_pct"] = marginPct;  // For reference/editing
```

This saves to `project.json`, e.g.:
```json
{
  "resolution": [1920, 1080],
  "canvas": [2304, 1296],  // For 20% margin: 1920 + 384, etc.
  "canvas_margin_pct": 20,
  ...
}
```

#### 3. Extend `Shot` Struct for Per-Shot Overrides
In `ScriptBreakdown.h`, add to `struct Shot`:
```cpp
int res_width = 0;     // Override project resolution (0 = use project)
int res_height = 0;
int canvas_width = 0;  // Override project canvas (0 = use project)
int canvas_height = 0;
```

- When serializing/loading shots (e.g., in `loadScene`, `saveModifiedScenes`), include these in QJsonObject.

#### 4. Set Defaults in Shot Creation
- In `importScript()` / `ScriptBreakdown::breakdownScript()`: When creating shots, set:
```cpp
// Assuming access to projectJson or passed fps/res/canvas
shot.res_width = projectJson["resolution"].toArray()[0].toInt();
shot.res_height = projectJson["resolution"].toArray()[1].toInt();
shot.canvas_width = projectJson["canvas"].toArray()[0].toInt();
shot.canvas_height = projectJson["canvas"].toArray()[1].toInt();
```
- In `MainWindow::onNewShot()` / `insertShotSegment()`: Same as above for new `GameFusion::Shot newShot;`.

This ensures new shots inherit project defaults.

For overrides: Add UI (e.g., in ShotPanelWidget) to edit per-shot, then save/update.

#### 5. Adapt `PaintArea` to Dynamic Canvas Size
`PaintArea` currently hardcodes 1920x1080. Make it dynamic.

**Changes to `paintarea.h`**:
- Add setters:
```cpp
void setCanvasSize(int width, int height);
void setOutputResolution(int width, int height);  // For drawing centered frame guides
```
- Add members:
```cpp
int m_outputWidth = 1920;
int m_outputHeight = 1080;
```

**Changes to `paintarea.cpp`**:
- In constructor: Remove hardcoded sizes; use params or defaults.
```cpp
PaintArea::PaintArea(QWidget *parent, int initWidth = 1920, int initHeight = 1080) : ... {
    setCanvasSize(initWidth, initHeight);
}
void PaintArea::setCanvasSize(int width, int height) {
    m_baseSize = QSize(width, height);
    compositeImage = QImage(m_baseSize, QImage::Format_ARGB32_Premultiplied);
    compositeImage.fill(Qt::transparent);
    // Resize all layers (non-destructively: center old content)
    for (auto& layerUI : layersUI) {
        QImage newImg(m_baseSize, QImage::Format_ARGB32_Premultiplied);
        newImg.fill(Qt::transparent);
        QPainter p(&newImg);
        p.drawImage((width - layerUI.image.width()) / 2, (height - layerUI.image.height()) / 2, layerUI.image);
        layerUI.image = newImg;
        layerUI.imageDirty = true;
    }
    updateCompositeImage();
    updateGeometry();
    resize(m_baseSize);  // Resize widget
}
void PaintArea::setOutputResolution(int width, int height) {
    m_outputWidth = width;
    m_outputHeight = height;
}
```
- In `paintEvent()` / `renderScene()`: Draw output frame guides (dashed rect) centered on canvas.
```cpp
// In renderScene(QPainter& painter, bool isExportMode)
if (!isExportMode) {  // Guides only in edit mode
    QRectF outputFrame((width() - m_outputWidth) / 2.0, (height() - m_outputHeight) / 2.0, m_outputWidth, m_outputHeight);
    QPen guidePen(Qt::DashLine);
    guidePen.setColor(Qt::red);
    painter.setPen(guidePen);
    painter.drawRect(outputFrame);
    // Optional: Draw safe frame within outputFrame (using projectJson["safeFrame"])
}
```
- In `setPanel(const GameFusion::Panel &panel, ...)`:
```cpp
// Get shot from context (assume passed or accessible via MainWindow)
const GameFusion::Shot* shot = ...;  // Retrieve via UUID or param
int resW = (shot->res_width > 0) ? shot->res_width : projectJson["resolution"][0].toInt();
int resH = (shot->res_height > 0) ? shot->res_height : projectJson["resolution"][1].toInt();
int canvasW = (shot->canvas_width > 0) ? shot->canvas_width : projectJson["canvas"][0].toInt();
int canvasH = (shot->canvas_height > 0) ? shot->canvas_height : projectJson["canvas"][1].toInt();
setOutputResolution(resW, resH);
setCanvasSize(canvasW, canvasH);
// Proceed with panel setup
```

When exporting/rendering, crop to output resolution (centered).

#### 6. Additional Project Attributes
Add to `NewProjectDialog.ui` (in QFormLayout):
```xml
<!-- After "Director" -->
<item row="9" column="0"><widget class="QLabel" name="labelSubtitle"><property name="text"><string>Subtitle</string></property></widget></item>
<item row="9" column="1"><widget class="QLineEdit" name="subtitleEdit"/></item>

<item row="10" column="0"><widget class="QLabel" name="labelEpisodeFormat"><property name="text"><string>Episode Format</string></property></widget></item>
<item row="10" column="1"><widget class="QLineEdit" name="episodeFormatEdit"><property name="placeholderText"><string>e.g., S01E01</string></property></widget></item>

<item row="11" column="0"><widget class="QLabel" name="labelCopyright"><property name="text"><string>Copyright</string></property></widget></item>
<item row="11" column="1"><widget class="QLineEdit" name="copyrightEdit"><property name="placeholderText"><string>e.g., © 2025 Your Company</string></property></widget></item>

<item row="12" column="0"><widget class="QLabel" name="labelStartTC"><property name="text"><string>Start Timecode</string></property></widget></item>
<item row="12" column="1"><widget class="QLineEdit" name="startTCEdit"><property name="placeholderText"><string>e.g., 01:00:00:00</string></property></widget></item>
```

**In `NewProjectDialog.h`**: Add getters `QString subtitle() const;`, etc.

**In `NewProjectDialog.cpp`**: Implement getters like `projectName()`.

**In `MainWindow::newProject()`**: Add to `projectJson`:
```cpp
projectJson["subtitle"] = dialog.subtitle();
projectJson["episode_format"] = dialog.episodeFormat();
projectJson["copyright"] = dialog.copyright();
projectJson["start_tc"] = dialog.startTC();  // Use in timeline (e.g., offset TC display)
```

- In `TimelineView` or export: Use "start_tc" for timecode offsets.

This integrates seamlessly. If needed, add UI for editing project settings post-creation. Let me know for more code details!
