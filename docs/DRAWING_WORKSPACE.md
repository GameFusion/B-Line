# Drawing workspace

Open **Windows → Drawing Workspace** (also shown at startup). This is the
independent `PaintCanvas` Graphics View window. Its raster QWidget viewport
shares the integrated `PaintArea` document and editing controller.

- Draw, text, select, erase, edit Bézier controls, manipulate layers and cameras.
- Brush attributes, active layer, visibility, panel/time selection and undo/redo
  come from the same controller and project as the integrated canvas.
- Wheel zooms around the pointer. Trackpad scrolling pans; Control/Command-wheel
  zooms. Space-drag or the middle button pans. **F** / **Fit** frames the canvas.
- Resizing preserves zoom. Workspace navigation never resizes the document or
  changes the integrated view's zoom. Drawing can extend beyond the output and
  overscan boundaries; export still uses the project's output/camera framing.
- Light table and camera preview are available in the workspace toolbar.
- Closing and reopening the window retains the document and navigation state.

## Implementation

`MainWindow` binds `PaintCanvas` to `MainWindowPaint::getPaintArea()` and shares
its history actions. Input maps through the view transform into the controller's
coordinates, including tablet pressure. Edits therefore use the normal project
signals, persistence and undo paths rather than maintaining a second panel.

`PaintArea::workspacePicture()` records the shared painter overlays and draws
cached stroke/text/image commands for layers without a canvas-sized clip.
Layer opacity and blend modes flatten only the visible portion when necessary;
this avoids applying opacity repeatedly where stroke segments overlap. Cache
invalidation is driven by document and controller updates. There is no polling
loop, OpenGL viewport, per-line graphics item or competing stroke worker.

The source renderer retains its existing layer animation, camera and export
semantics. This change does not redesign the integrated renderer's algorithms.
The workspace view is limited to +/- 1,000,000 logical pixels, with 2–3200% zoom.

## Verification

Build Boarder, then run from `build-vs2019-qt6/build/<configuration>`:

```sh
make -f ../../../tests/PaintCanvasSmoke.mk PaintCanvasSmoke
QT_QPA_PLATFORM=offscreen ./PaintCanvasSmoke
./PaintCanvasSmoke
```

The fixture checks negative/off-canvas drawing, shared edits, selection/deletion,
independent navigation, resize stability, layer visibility/opacity/compositing,
images, text, animation, camera interaction, synthetic tablet input, panel
switching, empty panels and controller detachment. It writes diagnostic renders
to `/tmp/boarder-workspace-*.png` and reports a cached 300-stroke redraw timing.
Native stylus hardware and large production-project playback still need user
acceptance; the synthetic benchmark is not a production performance guarantee.
