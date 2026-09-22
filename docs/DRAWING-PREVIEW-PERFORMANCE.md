# Drawing preview performance — 22 September 2026

This follow-up reduces long pressure-stroke repaint costs in both the integrated
canvas and the standalone graphics viewport. The existing fitting worker now
also prepares a viewport-sized stroke image; the GUI paints the immediate pen
tail and reuses that image. Finished strokes retain the original vector renderer,
geometry and pressure data.

Shared dependency: plugandpaint `master` commit `71f8528`. B-Line compiles its
sources directly from the sibling checkout; update both repositories together.
This report follows [STROKE-LATENCY.md](STROKE-LATENCY.md) and preserves that earlier
measurement record.

## Measured results

Apple M3 Max, macOS 26.6.2, Qt 6.10.2, existing **Debug build** (`-g`, no optimization
flag). The paired raster runs use Qt offscreen at 2x density, a 640 x 480 document,
an 800 x 600 workspace and a precise 4 ms input timer. The baseline is B-Line
`926d7cd` with plugandpaint `41f9a01` and TimeLineProject `ad3d477`.

These timings measure synthetic input dispatch to **Qt paint completion**,
including the immediate raw pen tail. They do not measure physical tablet input,
display scanout, or the delay until a newly refined fitted preview arrives from
the worker. Actual delivered timer intervals are retained in the data. All 5,632
samples per paired run were accepted and painted; all 52 strokes committed.

| Canvas / background | Gesture | Input-to-paint p95 before | After |
| --- | --- | ---: | ---: |
| Integrated / empty | 1,024 points | 9.35 ms | 1.16 ms |
| Integrated / 300-stroke light table | 1,024 points | 9.24 ms | 0.79 ms |
| Workspace / empty | 1,024 points | 12.50 ms | 1.31 ms |
| Workspace / 300-stroke light table | 1,024 points | 12.62 ms | 1.28 ms |
| Integrated / empty | 12 x 32 points | 2.10 ms | 2.98 ms |
| Integrated / 300-stroke light table | 12 x 32 points | 0.76 ms | 2.01 ms |
| Workspace / empty | 12 x 32 points | 1.44 ms | 2.24 ms |
| Workspace / 300-stroke light table | 12 x 32 points | 1.49 ms | 1.32 ms |

Short-stroke p95 does **not** improve across the board. The major gains are long
strokes and removal of repeated dense-background stalls: the maximum during the
short dense fixtures falls from **46.09 to 4.84 ms** in the integrated canvas and
**50.71 to 3.60 ms** in the workspace. A fresh viewport/background can still require
an expensive initial fill.

The harness now separately records release-to-first-committed-paint. For the four
long raster cases this is **12.99, 23.25, 25.33 and 25.07 ms**, respectively. This
metric was not available in the baseline; it is not interchangeable with the old
release-to-model-commit measurement and does not include all secondary windows.
The remaining final document rasterization cost is visible here.

Native Cocoa dense-workspace checks accept and paint all 1,024 points in both
runs: p95 **14.27 to 2.39 ms**. Native maximum latency remains **70.81 ms** after the
change (54.20 ms before), so the raster fixture's 4–5 ms maxima must not be read as
a native worst-case guarantee. Native release-to-committed-paint is 29.22 ms.

Additional synthesized Qt tablet events vary pressure from 0.05 to 0.95. The dense
long-stroke p95 is **0.73 ms integrated / 1.26 ms workspace** offscreen, and
**1.21 ms integrated** on Cocoa. These are post-change checks only; they are not
measurements from a physical stylus.

Raw numeric distributions and counts are in
[benchmarks/drawing-preview-2026-09-22.json](benchmarks/drawing-preview-2026-09-22.json).
Runs are sequential to avoid build/test CPU competition. These are individual
paired fixture runs, not a statistical performance guarantee.

## Implementation

- Extract the existing active stroke renderer into `StrokeRenderer.h`, unchanged
  apart from an explicit zoom argument. Both document drawing and worker previews
  use it. The worker owns its private `QImage` and painter; no QWidget work moves
  off the GUI thread. Qt documents this supported approach in
  [Painting in Threads](https://doc.qt.io/qt-6.10/threads-modules.html#painting-in-threads).
- Render previews only for curves with at least 128 assessed vertices. Limit each
  bitmap to 16,777,216 pixels (64 MiB) and permit at most one unacknowledged bitmap
  per worker. Results without a matching preview use the vector renderer. This
  bounds queued image storage without dropping input samples.
- Include visible bounds, zoom and pixel density in preview identity. A pan or
  resize falls back to vector drawing until the worker catches up, including
  when the pen is stationary. Acknowledgment also catches up a newer fit that
  arrived while an earlier bitmap was in flight. Draw the bitmap at its original pixel spacing,
  accounting for canvas margins and ceil-rounded allocation sizes.
- Paint live ink directly in `PaintCanvas`, outside QPicture recording. This
  avoids serializing a preview bitmap into every new command picture. Export and
  independent reference recordings retain vector drawing.
- Retain the unchanged scene background between gestures and while idle. Keep
  the faded light-table group (reference image and non-active layers) separately
  cached, so new ink on the active layer does not rerasterize all background
  strokes. Bounds, density, transforms, image revisions, visibility, opacity,
  blend modes, effects and active-layer changes invalidate the relevant cache.
- On release, let the final worker result commit and refresh the document instead
  of first repainting the old document. Other editor previews still remain
  deferred during the held gesture. Final vectors use the existing document path.

The worker still renders a complete fitted preview at its coalesced cadence; this
pass relocates that raster cost instead of eliminating it. Further work can
measure fitted-preview delivery latency and reduce the final committed raster
cost. Different editor viewport sizes currently share a background cache slot,
which can require rebuilding when both editors refresh after release.

## Validation

- Build of Boarder and the affected smoke-test executables: passed.
- `StrokeLatency --verify`: 1,095 fitting-equivalence checks passed.
- `StrokePreviewSmoke`: 214 checks passed on offscreen and Cocoa. Covers changing
  pressure, uniform/pressure/taper width, solid/gradient translucent colours,
  three zooms, 1x/2x density, panned bounds, margins, target changes, backpressure,
  stale-preview fallback, oversized allocation rejection and final geometry.
  Maximum pixel-channel difference from direct drawing is **2/255**, due to
  premultiplied alpha rounding; geometry/pressure fingerprints remain identical.
- `PaintCanvasSmoke`: 86 checks passed on offscreen and Cocoa. The cached light
  table is compared against the independent full-scene rendering path. Existing
  layer, animation, interaction, late-result and export checks also pass.
- `PlaybackSmoke`: 40 offscreen checks and 45 native checks with audio passed.
- `ExportMovieSmoke`: passed actual rendering, encoding and movie-player opening
  with a temporary silent fixture.
- Native playback regression on a disposable copy of the supplied 60-shot,
  63-panel, 24 fps project: **23.93 fps**, all 63 panels painted, zero frames in
  the inactive viewport. This is playback regression evidence, not a drawing
  benchmark on production artwork. The original project's 43 recorded JSON/FDX
  content hashes remain unchanged. No existing user application was restarted.

The rebuilt application must be relaunched to use these changes. Physical stylus
feel, long-duration audio synchronization and Windows remain separate acceptance
work.

## Reproduction

From `build-vs2019-qt6/build/Qt_6_10_2_for_macOS-Debug`:

```sh
make -j6 -f ../../../tests/PaintCanvasSmoke.mk all StrokeLatency StrokePreviewSmoke PaintCanvasSmoke PlaybackSmoke ExportMovieSmoke
QT_QPA_PLATFORM=offscreen ./StrokeLatency --verify
QT_QPA_PLATFORM=offscreen ./StrokePreviewSmoke
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=2 ./StrokeLatency
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=2 ./StrokeLatency --ui --dense --tablet
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=2 ./StrokeLatency --ui --workspace --dense --tablet
QT_QPA_PLATFORM=offscreen ./PaintCanvasSmoke
QT_QPA_PLATFORM=offscreen ./PlaybackSmoke
QT_QPA_PLATFORM=offscreen ./ExportMovieSmoke
./StrokeLatency --ui --workspace --dense
./StrokePreviewSmoke
./PaintCanvasSmoke
./PlaybackSmoke --audio
```

Keep native fixture windows visible; the timing harness rejects missing input
or paint samples. Run timing fixtures without simultaneous builds or benchmarks.
