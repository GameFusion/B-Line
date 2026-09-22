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
- The toolbar uses Font Awesome icons matching the integrated editor, with
  tooltips, blue selected tools and an amber Light table toggle. Navigation,
  camera preview and Undo/Redo also use icons; shared history shortcuts remain.
- Light table fades the reference image and non-active layers together to 25%,
  after compositing their overlaps and blend modes. The active layer stays on
  top with its authored transform, opacity and visibility. Both editor windows
  share the light-table setting and selected tool; newly opened workspaces
  inherit them. Camera previews, timeline thumbnails and movie output keep the
  full scene, including the active layer.
- Playback renders only through the focused visible editor. Closing, hiding or
  minimizing that window transfers playback to the other visible canvas; when
  both are hidden, rendering pauses while the clock continues. Pause/Stop restore
  both views. The inactive canvas retains its last snapshot.
- During playback, the active preview/canvas shows measured / target FPS. The
  main transport also names the viewport receiving frames. Counts reflect
  distinct painted timeline frames, not timer ticks or export rendering.
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

The two editors share light-table composition. Its temporary background surface
is bounded to the visible region and rendered at the view's display density;
vector layer commands remain cached. The full document composite remains
independent of light-table mode for camera previews, thumbnails and exports.
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
The light-table regression fixture covers the panel reference, image layers,
blended overlaps, off-canvas ink, active-layer opacity/visibility, panel changes,
export isolation, initial toggle state and shared history actions. The native
window capture is `/tmp/boarder-workspace-lighttable.png`. PlaybackSmoke checks
mode synchronization through both actual MainWindow toolbars.

Native stylus hardware and large production-project playback still need user
acceptance; the synthetic benchmark is not a production performance guarantee.
